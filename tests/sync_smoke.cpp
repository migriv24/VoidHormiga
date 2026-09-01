/* sync_smoke.cpp — the merge and the LAN transport, tested against the ways
 * they can be quietly wrong.
 *
 * This is the phase F suite that okf/concepts/platform/data-planes.md §7 asked
 * for: "two folders on one machine and a file passed between them. No second
 * computer, no network, no encryption, no Phase 4. If the merge is wrong, that
 * is where it is cheapest to find out." The author's own stated fear was
 * duplicated data, so that is what most of this is about.
 *
 * THREE THINGS HERE ARE PINS RATHER THAN ASPIRATIONS, and they are marked:
 *
 *   1. A DELETION DOES NOT PROPAGATE. This increment enriches from bare state
 *      on every merge, so it has no memory of what this peer once observed, and
 *      "I removed it" is indistinguishable from "you added it". The test
 *      asserts the CURRENT behaviour on purpose — so the day someone adopts
 *      Palabra's container and fixes it, this test fails and tells them the
 *      limitation is gone rather than letting it change unnoticed.
 *   2. The one-sided report is the mitigation for (1) and is asserted to be
 *      complete, because it is the only thing standing between a volunteer and
 *      a resurrected contact.
 *   3. A truncated sealed payload must FAIL, not return its prefix — the same
 *      property backup_smoke.cpp exists to defend, at the network seam.
 *
 * The socket half runs on loopback in one process, two threads. That proves the
 * handshake, the key agreement, the SAS agreement and the framing; it does NOT
 * prove two machines on one Wi-Fi, and the concept page says so.
 */
#include "sync/merge.hpp"
#include "sync/peer.hpp"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace hormiga::sync;

static int failures = 0;
static void ok(bool cond, const char* what) {
    std::printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) ++failures;
}

/* A minimal but REAL Void Core state document — the shape `export_state()`
 * produces, down to the keys the merge must preserve (`config`, `domains`)
 * and the ones it must replace (`mantles`). */
static std::string state(const std::string& runes, const char* base_url = "https://a.example") {
    return std::string("{\"active\":\"org\",\"version\":1,") +
           "\"config\":{\"actor\":\"human:hormiga\",\"site\":{\"base_url\":\"" + base_url +
           "\"}}," + "\"domains\":[],\"bindings\":[],\"scripts\":[]," +
           "\"mantles\":[{\"name\":\"org\",\"domain\":\"\",\"tags\":[],\"runes\":[" + runes +
           "]}]}";
}
static std::string rune(const char* id, const char* name, const char* phone,
                        const char* tags = "\"type:contact\"") {
    return std::string("{\"spirit\":{\"id\":\"") + id + "\",\"name\":\"" + name +
           "\"},\"glyph\":\"contact\",\"relations\":[],\"placement\":null," +
           "\"content\":{\"phone\":\"" + phone + "\"},\"tags\":[" + tags + "]}";
}

static bool has_conflict_on(const MergeResult& r, const char* field) {
    for (const auto& c : r.conflicts)
        if (c.field.find(field) != std::string::npos) return true;
    return false;
}
static bool reports_one_sided(const MergeResult& r, const char* rune_id, bool on_local) {
    for (const auto& o : r.one_sided)
        if (o.rune == rune_id && o.on_local == on_local) return true;
    return false;
}
static bool state_mentions(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

/* ── 1. version names ───────────────────────────────────────────────────── */
static void test_version_names() {
    std::printf("version names\n");
    const std::string a = state(rune("rune_1", "ada", "555-0001"));

    ok(!version_name(a).empty(), "a real state document has a version name");
    ok(version_name(a) == version_name(a), "the same document names itself the same way");
    ok(version_name(a).rfind("v:", 0) == 0, "the name is a Palabra cut name (v:...)");

    /* THE POINT OF THE BEACON. Two peers who reached the same content by
     * different routes must agree; a different rune ORDER is the same content
     * (Void Core SPEC 4: rune order is preserved but not semantic). */
    const std::string two_ab =
        state(rune("rune_1", "ada", "555-0001") + "," + rune("rune_2", "bo", "555-0002"));
    const std::string two_ba =
        state(rune("rune_2", "bo", "555-0002") + "," + rune("rune_1", "ada", "555-0001"));
    ok(version_name(two_ab) == version_name(two_ba),
       "rune order does not change the version name");

    const std::string edited = state(rune("rune_1", "ada", "555-9999"));
    ok(version_name(a) != version_name(edited), "one edited field changes the version name");

    /* config is peer-local resolution and is NOT in the versioned slice, so a
     * device with a different site URL is still the same cut. */
    ok(version_name(a) == version_name(state(rune("rune_1", "ada", "555-0001"),
                                             "https://different.example")),
       "config is outside the versioned slice, so it does not move the name");

    std::string err;
    ok(version_name("{not json", &err).empty() && !err.empty(),
       "a malformed document reports rather than crashes");
}

/* ── 2. the merge ───────────────────────────────────────────────────────── */
static void test_merge_basics() {
    std::printf("merge\n");
    const std::string a = state(rune("rune_1", "ada", "555-0001"));

    MergeResult same = merge_states(a, a, "peerA", "peerB");
    ok(same.ok && same.identical, "two identical states report identical, and merge trivially");
    ok(same.conflicts.empty(), "identical states produce no conflicts");

    /* THE GUARD ON THE MINT. Two peers minting the same tags is a corruption
     * that presents as a silent drop, so it is refused rather than defaulted. */
    MergeResult bad = merge_states(a, a, "peerA", "peerA");
    ok(!bad.ok && !bad.error.empty(), "identical peer ids are refused, not accommodated");
    ok(!merge_states(a, "{oops", "peerA", "peerB").ok, "a malformed peer document is refused");
}

static void test_merge_one_sided_edit() {
    std::printf("merge: one side edited\n");
    const std::string a = state(rune("rune_1", "ada", "555-0001"));
    const std::string b = state(rune("rune_1", "ada", "555-7777"));

    MergeResult r = merge_states(a, b, "peerA", "peerB");
    ok(r.ok, "the merge completes");
    ok(!r.identical, "different states are not reported identical");
    ok(r.runes == 1, "one rune in, one rune out - no duplication");

    /* The author's stated fear, asserted directly: a merge must not turn one
     * contact into two. */
    ok(!state_mentions(r.merged_state, "rune_1\",\"name\":\"ada\"},\"glyph\":\"contact\","
                                       "\"relations\":[],\"placement\":null,\"content\":"
                                       "{\"phone\":\"555-0001\"}}"),
       "the merged document does not carry both copies of the rune verbatim");
}

static void test_merge_conflict() {
    std::printf("merge: concurrent edits to one field\n");
    /* Both peers changed the SAME field to DIFFERENT values. Neither is right,
     * and the system's whole posture is that a human settles it. */
    const std::string a = state(rune("rune_1", "ada", "555-1111"));
    const std::string b = state(rune("rune_1", "ada", "555-2222"));

    MergeResult r = merge_states(a, b, "peerA", "peerB");
    ok(r.ok, "a conflicting merge still completes - a conflict is a value, not an error");
    ok(!r.conflicts.empty(), "the conflict is REPORTED rather than resolved silently");
    ok(has_conflict_on(r, "phone"), "the conflict names the field that diverged");
    if (!r.conflicts.empty()) {
        ok(r.conflicts[0].sides.size() == 2, "both surviving values are carried");
        ok(!r.conflicts[0].hash.empty(),
           "the conflict has its own content address, so two peers name it identically");
    }
}

static void test_merge_addition() {
    std::printf("merge: an addition on one side\n");
    const std::string a = state(rune("rune_1", "ada", "555-0001"));
    const std::string b =
        state(rune("rune_1", "ada", "555-0001") + "," + rune("rune_2", "bo", "555-0002"));

    MergeResult r = merge_states(a, b, "peerA", "peerB");
    ok(r.ok && r.runes == 2, "the peer's new rune arrives");
    ok(state_mentions(r.merged_state, "rune_2"), "and is present in the merged document");
    ok(reports_one_sided(r, "rune_2", /*on_local=*/false),
       "and is reported as having come from the peer");
    ok(r.conflicts.empty(), "an addition is not a conflict");
}

/* ── 3. THE PIN: deletions do not propagate, and the report is the mitigation */
static void test_deletion_does_not_propagate() {
    std::printf("merge: deletion (KNOWN LIMITATION, pinned)\n");
    /* Local deleted rune_2. The peer still has it. A stateless join cannot tell
     * this apart from "the peer just added rune_2", so the rune comes back. */
    const std::string local = state(rune("rune_1", "ada", "555-0001"));
    const std::string peer =
        state(rune("rune_1", "ada", "555-0001") + "," + rune("rune_2", "bo", "555-0002"));

    MergeResult r = merge_states(local, peer, "peerA", "peerB");
    ok(r.ok, "the merge completes");
    ok(state_mentions(r.merged_state, "rune_2"),
       "PINNED: the deleted rune RETURNS - a fresh enrich has no memory of the removal");
    ok(reports_one_sided(r, "rune_2", /*on_local=*/false),
       "but it is reported one-sided, which is the only reason this is shippable");

    /* Symmetry: the same observation from the other direction. */
    MergeResult back = merge_states(peer, local, "peerA", "peerB");
    ok(back.ok && reports_one_sided(back, "rune_2", /*on_local=*/true),
       "and from the other side it is reported as ours alone");
    ok(r.version_merged == back.version_merged,
       "the merge is commutative: both orders reach the same cut");
}

/* ── 4. what must NOT be merged ─────────────────────────────────────────── */
static void test_config_is_not_merged() {
    std::printf("merge: peer-local resolution stays local\n");
    const std::string a = state(rune("rune_1", "ada", "555-0001"), "https://ours.example");
    const std::string b = state(rune("rune_1", "ada", "555-7777"), "https://theirs.example");

    MergeResult r = merge_states(a, b, "peerA", "peerB");
    ok(r.ok, "the merge completes");
    ok(state_mentions(r.merged_state, "https://ours.example"),
       "our own site.base_url survives the merge");
    ok(!state_mentions(r.merged_state, "https://theirs.example"),
       "the peer's config does NOT arrive - syncing cannot move a device's backends");
    ok(state_mentions(r.merged_state, "\"active\":\"org\""),
       "the rest of the local document is preserved around the spliced mantles");
}

/* ── 5. the short authentication string ─────────────────────────────────── */
static void test_sas() {
    std::printf("short authentication string\n");
    KeyPair a = generate_keypair(), b = generate_keypair(), c = generate_keypair();
    ok(a.public_key.size() == 32 && a.secret_key.size() == 32, "keypairs are X25519-sized");
    ok(a.public_key != b.public_key, "two keypairs differ");

    const std::string ab = short_auth_string(a.public_key, b.public_key);
    ok(ab.size() == 6, "the SAS is six characters - short enough to read aloud");
    /* THE PROPERTY THAT MAKES IT COMPARABLE OUT OF BAND: both ends compute it
     * without agreeing who goes first. */
    ok(ab == short_auth_string(b.public_key, a.public_key),
       "the SAS is symmetric, so both ends compute the same string");
    ok(ab != short_auth_string(a.public_key, c.public_key),
       "a substituted key changes it - which is what makes a machine-in-the-middle visible");

    for (char ch : ab)
        if (ch == '0' || ch == 'O' || ch == '1' || ch == 'I' || ch == 'L')
            ok(false, "the alphabet excludes characters that are misheard aloud");
    ok(true, "the alphabet excludes characters that are misheard aloud");
    ok(short_auth_string("short", b.public_key).empty(), "a malformed key yields no SAS");
    ok(fingerprint_of(a.public_key).size() == 16 &&
           fingerprint_of(a.public_key) != fingerprint_of(b.public_key),
       "fingerprints are short and distinguish keys");
}

/* ── 6. the sealed blob, including the failure a single-shot seal misses ── */
static void test_seal() {
    std::printf("sealed payloads\n");
    std::string key(32, '\0');
    for (int i = 0; i < 32; i++) key[i] = (char)(i * 7 + 1);
    std::string other = key;
    other[0] = (char)0x55;

    /* Multi-chunk on purpose: framing bugs live at the boundaries. */
    std::string plain;
    for (int i = 0; i < 200000; i++) plain.push_back((char)('a' + (i % 26)));

    std::string sealed, out, err;
    ok(seal_blob(plain, key, sealed), "a payload seals");
    ok(sealed.size() > plain.size(), "the sealed form carries its header and tags");
    ok(open_blob(sealed, key, out, &err) && out == plain, "and round-trips exactly");

    ok(!open_blob(sealed, other, out, &err), "a wrong key is refused");
    std::string tampered = sealed;
    tampered[tampered.size() / 2] ^= 0x01;
    ok(!open_blob(tampered, key, out, &err), "a single flipped byte is refused");

    /* THE ONE THAT MATTERS. Every byte of a truncated stream is authentic, so
     * per-chunk authentication cannot see it; only the FINAL tag can. Returning
     * a plausible prefix of somebody's database is the quiet failure. */
    std::string cut = sealed.substr(0, sealed.size() / 2);
    out.clear();
    err.clear();
    ok(!open_blob(cut, key, out, &err), "a TRUNCATED payload is refused, not returned in part");
    ok(err.find("truncated") != std::string::npos, "and it says truncated, so the log is useful");
}

/* ── 7. the beacon payload ──────────────────────────────────────────────── */
static void test_beacon_payload() {
    std::printf("beacon payload\n");
    PeerInfo self;
    self.peer_id = "ada-laptop";
    self.display = "Ada's laptop";
    self.version = "v:deadbeefcafe";
    self.fingerprint = "0011223344556677";
    self.port = 47732;

    PeerInfo got;
    ok(decode_beacon(encode_beacon(self), got), "a beacon round-trips");
    ok(got.peer_id == self.peer_id && got.version == self.version && got.port == self.port,
       "and carries the id, the cut name and the port");
    ok(got.display == self.display, "including a display name with an apostrophe in it");
    ok(got.last_seen > 0, "the receiver stamps when it heard it");

    /* An open UDP port receives whatever is on the network. Malformed input is
     * the expected case here, not the exceptional one. */
    ok(!decode_beacon("", got), "an empty datagram is rejected");
    ok(!decode_beacon("hello there", got), "random text is rejected");
    ok(!decode_beacon("{\"app\":\"somethingelse\",\"p\":1,\"id\":\"x\",\"v\":\"y\",\"port\":1}",
                      got),
       "another application's datagram is rejected");
    ok(!decode_beacon("{\"app\":\"voidhormiga\",\"p\":99,\"id\":\"x\",\"v\":\"y\",\"port\":1}",
                      got),
       "a future protocol version is rejected rather than half-parsed");
    ok(!decode_beacon("{\"app\":\"voidhormiga\",\"p\":1,\"v\":\"y\",\"port\":1}", got),
       "a beacon with no peer id is rejected");
    ok(!decode_beacon("{\"app\":\"voidhormiga\",\"p\":1,\"id\":\"x\",\"v\":\"y\",\"port\":99999}",
                      got),
       "an out-of-range port is rejected");
    ok(!decode_beacon(std::string("{\"app\":\"voidhormiga\",\"p\":1,\"id\":\"") +
                          std::string(4000, 'x') + "\",\"v\":\"y\",\"port\":1}",
                      got),
       "an oversized datagram is rejected before it is parsed");
}

/* ── 8. a real handshake and transfer, on loopback ──────────────────────── */
static void test_loopback_session() {
    std::printf("loopback session\n");
    const std::uint16_t port = 47799;
    KeyPair server = generate_keypair(), client = generate_keypair();

    std::string s_peer_pk, s_sas, s_err, s_got;
    bool s_ok = false, s_recv = false, s_send = false;
    std::thread listener([&] {
        Session in;
        s_ok = in.accept_one(port, server, s_peer_pk, s_sas, 5000, &s_err);
        if (!s_ok) return;
        s_recv = in.receive(s_got, &s_err);
        s_send = in.send("pong: " + s_got, &s_err);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    Session out;
    std::string c_peer_pk, c_sas, c_err;
    /* A payload larger than one chunk, because a state document is. */
    const std::string payload(150000, 'q');
    bool c_ok = out.connect("127.0.0.1", port, client, c_peer_pk, c_sas, &c_err);
    bool c_send = c_ok && out.send(payload, &c_err);
    std::string reply;
    bool c_recv = c_send && out.receive(reply, &c_err);
    out.close();
    listener.join();

    ok(c_ok, c_ok ? "the client connects and completes the handshake" : c_err.c_str());
    ok(s_ok, s_ok ? "the listener accepts and completes the handshake" : s_err.c_str());
    if (c_ok && s_ok) {
        ok(c_peer_pk == server.public_key && s_peer_pk == client.public_key,
           "each side learned the other's real public key");
        /* The whole security property, asserted: both ends independently
         * compute the same six characters for two humans to compare. */
        ok(!c_sas.empty() && c_sas == s_sas, "both ends derive the SAME short auth string");
        ok(c_sas == short_auth_string(client.public_key, server.public_key),
           "and it matches the SAS computed from the two public keys directly");
        ok(c_send && s_recv && s_got == payload,
           "a multi-chunk payload crosses the sealed channel intact");
        ok(c_recv && reply == "pong: " + payload, "and the reverse direction works too");
    }

    Session dead;
    std::string pk, sas, err;
    ok(!dead.connect("127.0.0.1", 1, client, pk, sas, &err) && !err.empty(),
       "connecting to a closed port reports rather than hangs");
    ok(!dead.send("x", &err), "a session that never opened refuses to send");
}

int main() {
    std::printf("sync smoke\n");
    test_version_names();
    test_merge_basics();
    test_merge_one_sided_edit();
    test_merge_conflict();
    test_merge_addition();
    test_deletion_does_not_propagate();
    test_config_is_not_merged();
    test_sas();
    test_seal();
    test_beacon_payload();
    test_loopback_session();
    std::printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
