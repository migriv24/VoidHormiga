/* vault.cpp — see vault.hpp. libsodium (argon2id + XChaCha20-Poly1305). */
#include "platform/vault.hpp"

#include "sodium.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace hormiga {

// ── base64 (libsodium's, URL-safe-agnostic std variant) ─────────────────────

static std::string b64(const unsigned char* data, size_t n) {
    std::string out(sodium_base64_encoded_len(n, sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(out.data(), out.size(), data, n,
                      sodium_base64_VARIANT_ORIGINAL);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

static std::vector<unsigned char> unb64(const std::string& s, bool& ok) {
    std::vector<unsigned char> out(s.size()); // decoded is never larger
    size_t n = 0;
    ok = sodium_base642bin(out.data(), out.size(), s.c_str(), s.size(), nullptr,
                           &n, nullptr, sodium_base64_VARIANT_ORIGINAL) == 0;
    out.resize(ok ? n : 0);
    return out;
}

// ── a deliberately tiny JSON reader for the flat envelope (no nesting) ──────
// The envelope is our own format: a flat object of string/number values we
// wrote ourselves, so a full JSON lib is overkill here. Values never contain
// quotes (base64 + integers). Secrets are inside the CIPHERTEXT, parsed with
// nlohmann/json in the app — this only cracks the outer envelope.

static std::string field(const std::string& doc, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto p = doc.find(pat);
    if (p == std::string::npos) return {};
    p = doc.find(':', p + pat.size());
    if (p == std::string::npos) return {};
    ++p;
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\t')) ++p;
    if (p < doc.size() && doc[p] == '"') { // string
        auto e = doc.find('"', p + 1);
        return doc.substr(p + 1, e - p - 1);
    }
    auto e = doc.find_first_of(",}", p); // number
    std::string v = doc.substr(p, e - p);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\n')) v.pop_back();
    return v;
}

/* Serialize the in-memory secrets map to a compact JSON object (the plaintext
 * that gets sealed). Keys/values are our own (API keys, hex/base64-ish); we
 * escape quotes and backslashes so a value can't break the structure. */
static std::string secrets_json(const std::map<std::string, std::string>& s) {
    std::string out = "{";
    bool first = true;
    auto esc = [](const std::string& v) {
        std::string o;
        for (char c : v) {
            if (c == '"' || c == '\\') o += '\\';
            o += c;
        }
        return o;
    };
    for (const auto& [k, v] : s) {
        if (!first) out += ",";
        first = false;
        out += "\"" + esc(k) + "\":\"" + esc(v) + "\"";
    }
    return out + "}";
}

static std::map<std::string, std::string> parse_secrets(const std::string& j) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    auto read_str = [&](size_t& p) -> std::string {
        std::string s;
        ++p; // opening quote
        while (p < j.size() && j[p] != '"') {
            if (j[p] == '\\' && p + 1 < j.size()) ++p;
            s += j[p++];
        }
        ++p; // closing quote
        return s;
    };
    while (i < j.size()) {
        if (j[i] == '"') {
            std::string k = read_str(i);
            while (i < j.size() && j[i] != '"' && j[i] != '}') ++i;
            if (i < j.size() && j[i] == '"') {
                std::string v = read_str(i);
                out[k] = v;
            }
        } else {
            ++i;
        }
    }
    return out;
}

// ── lifecycle ────────────────────────────────────────────────────────────────

Vault::~Vault() { lock(); }

bool Vault::global_init() { return sodium_init() >= 0; }

bool Vault::exists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

void Vault::lock() {
    if (!passphrase_.empty())
        sodium_memzero(passphrase_.data(), passphrase_.size());
    passphrase_.clear();
    for (auto& [k, v] : secrets_)
        if (!v.empty()) sodium_memzero(v.data(), v.size());
    secrets_.clear();
    unlocked_ = false;
}

bool Vault::create(const std::string& passphrase) {
    lock();
    salt_.resize(crypto_pwhash_SALTBYTES);
    randombytes_buf(salt_.data(), salt_.size());
    ops_ = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    mem_ = crypto_pwhash_MEMLIMIT_INTERACTIVE;
    passphrase_ = passphrase;
    unlocked_ = true;
    return true;
}

void Vault::rekey(const std::string& new_passphrase) {
    if (!passphrase_.empty())
        sodium_memzero(passphrase_.data(), passphrase_.size());
    passphrase_ = new_passphrase;
    // fresh salt on rekey so the same key can't be re-derived from an old salt
    salt_.resize(crypto_pwhash_SALTBYTES);
    randombytes_buf(salt_.data(), salt_.size());
}

std::string Vault::get(const std::string& key) const {
    auto it = secrets_.find(key);
    return it == secrets_.end() ? std::string() : it->second;
}
void Vault::set(const std::string& key, const std::string& value) {
    secrets_[key] = value;
}
void Vault::erase(const std::string& key) { secrets_.erase(key); }
std::vector<std::string> Vault::keys() const {
    std::vector<std::string> k;
    for (const auto& [key, _] : secrets_) k.push_back(key);
    return k;
}

// ── the crypto: derive → seal / open ────────────────────────────────────────

static bool derive_key(const std::string& pass, const unsigned char* salt,
                       unsigned long long ops, unsigned long long mem,
                       unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]) {
    return crypto_pwhash(key, crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                         pass.c_str(), pass.size(), salt, ops, (size_t)mem,
                         crypto_pwhash_ALG_ARGON2ID13) == 0;
}

bool Vault::save(const std::string& path) {
    if (!unlocked_) { err_ = "vault is locked"; return false; }
    unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    if (!derive_key(passphrase_, salt_.data(), ops_, mem_, key)) {
        err_ = "key derivation failed (out of memory?)";
        return false;
    }
    std::string plain = secrets_json(secrets_);
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    randombytes_buf(nonce, sizeof nonce);
    std::vector<unsigned char> ct(plain.size() +
                                  crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long ct_len = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(
        ct.data(), &ct_len, (const unsigned char*)plain.data(), plain.size(),
        nullptr, 0, nullptr, nonce, key);
    ct.resize(ct_len);
    sodium_memzero(key, sizeof key);
    sodium_memzero(plain.data(), plain.size());

    std::ostringstream env;
    env << "{\n  \"magic\": \"MIGA\",\n  \"version\": 2,\n"
        << "  \"kdf\": \"argon2id\",\n"
        << "  \"ops\": " << ops_ << ",\n  \"mem\": " << mem_ << ",\n"
        << "  \"salt\": \"" << b64(salt_.data(), salt_.size()) << "\",\n"
        << "  \"nonce\": \"" << b64(nonce, sizeof nonce) << "\",\n"
        << "  \"ct\": \"" << b64(ct.data(), ct.size()) << "\"\n}\n";

    // write-to-temp-then-rename: a crash mid-write never corrupts the vault
    std::string tmp = path + ".tmp";
    { std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out << env.str();
      if (!out) { err_ = "write failed: " + tmp; return false; } }
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        err_ = "rename failed: " + path;
        return false;
    }
    return true;
}

bool Vault::unlock(const std::string& path, const std::string& passphrase) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { err_ = "no vault file"; return false; }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string env = ss.str();

    bool ok1 = false, ok2 = false, ok3 = false;
    auto salt = unb64(field(env, "salt"), ok1);
    auto nonce = unb64(field(env, "nonce"), ok2);
    auto ct = unb64(field(env, "ct"), ok3);
    unsigned long long ops = std::strtoull(field(env, "ops").c_str(), nullptr, 10);
    unsigned long long mem = std::strtoull(field(env, "mem").c_str(), nullptr, 10);
    if (!ok1 || !ok2 || !ok3 || salt.size() != crypto_pwhash_SALTBYTES ||
        nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES || ops == 0 ||
        mem == 0) {
        err_ = "corrupt vault envelope";
        return false;
    }

    unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    if (!derive_key(passphrase, salt.data(), ops, mem, key)) {
        err_ = "key derivation failed";
        return false;
    }
    std::vector<unsigned char> plain(ct.size()); // >= plaintext len
    unsigned long long plain_len = 0;
    int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plain.data(), &plain_len, nullptr, ct.data(), ct.size(), nullptr, 0,
        nonce.data(), key);
    sodium_memzero(key, sizeof key);
    if (rc != 0) { // wrong passphrase OR tampered file — indistinguishable
        err_ = "wrong passphrase or corrupt vault";
        return false;
    }
    std::string json((const char*)plain.data(), plain_len);
    sodium_memzero(plain.data(), plain.size());

    lock();
    salt_ = salt;
    ops_ = ops;
    mem_ = mem;
    passphrase_ = passphrase;
    secrets_ = parse_secrets(json);
    sodium_memzero(json.data(), json.size());
    unlocked_ = true;
    return true;
}

} // namespace hormiga
