/* aws_smoke.cpp — AWS Signature Version 4, checked as far as it can be checked
 * without an AWS account.
 *
 * ── WHAT THIS PROVES, AND WHAT IT CANNOT ─────────────────────────────────────
 *
 * A signing implementation that is confidently wrong fails identically to a bad
 * credential — an opaque 403 — so it is worth being exact about which of these
 * assertions are evidence and which are only self-consistency.
 *
 * EVIDENCE (independent vectors, from outside this project):
 *   - SHA-256 against FIPS 180-4's published digest for "abc";
 *   - HMAC-SHA-256 against RFC 4231 test cases 1 and 2.
 *   These two primitives are the whole of SigV4's cryptography. If they are
 *   right, nothing cryptographic here can be wrong; only the ORDER can be.
 *
 * STRUCTURE (checkable by reading the spec beside the test):
 *   - the canonical request is built field for field, with the exact newline
 *     placement AWS specifies — the part that is arbitrary, unguessable, and
 *     produces a 403 rather than a hint when wrong;
 *   - the signing key chain narrows in the documented order;
 *   - URI encoding follows RFC 3986 with uppercase hex, and leaves `/` alone in
 *     a path while encoding it in a query value.
 *
 * SENSITIVITY (the property that makes the above worth having):
 *   - changing any one input — a byte of the body, the date, the region, the
 *     key, a header — changes the signature. A signer that ignored an input
 *     would pass every structural check and be catastrophically wrong.
 *
 * NOT PROVEN HERE: that AWS agrees. Nothing offline can establish that. The
 * Cloudflare deploy taught this project the lesson already — the local half was
 * exercised end to end and the remote half was wrong by one HTTP verb, found in
 * a minute by an operator with real credentials because the vendor's error
 * reached the log unparsed. Same play: the operator drives the first real call.
 * AWS helps here, because a SigV4 refusal returns the canonical request AWS
 * computed, and `Signed` exposes ours to diff against it.
 */
#include "publish/aws.hpp"

#include <cstdio>
#include <string>

using namespace hormiga::aws;

static int failures = 0;
static void ok(bool cond, const char* what) {
    std::printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) ++failures;
}
static void eq(const std::string& got, const std::string& want, const char* what) {
    const bool good = got == want;
    std::printf("  %s %s\n", good ? "ok  " : "FAIL", what);
    if (!good) {
        std::printf("        want %s\n", want.c_str());
        std::printf("        got  %s\n", got.c_str());
        ++failures;
    }
}

int main() {
    std::printf("aws: the primitives, against published vectors\n");
    {
        // FIPS 180-4, the canonical one-block example
        eq(sha256_hex("abc"),
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
           "SHA-256(\"abc\") matches FIPS 180-4");
        eq(sha256_hex(""),
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
           "SHA-256(\"\") matches the well-known empty digest");

        // RFC 4231 test case 1: key = 0x0b x20, data = "Hi There"
        eq(hex(hmac_sha256(std::string(20, '\x0b'), "Hi There")),
           "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
           "HMAC-SHA-256 matches RFC 4231 case 1");
        // RFC 4231 test case 2: key = "Jefe"
        eq(hex(hmac_sha256("Jefe", "what do ya want for nothing?")),
           "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
           "...and RFC 4231 case 2 (a short, non-32-byte key)");
    }

    std::printf("aws: URI encoding\n");
    {
        eq(uri_encode("abcXYZ019-_.~", false), "abcXYZ019-_.~",
           "unreserved characters pass through");
        eq(uri_encode("a b", false), "a%20b", "a space is %20, never +");
        eq(uri_encode("index/directory.json", false), "index/directory.json",
           "a path keeps its slashes");
        eq(uri_encode("index/directory.json", true), "index%2Fdirectory.json",
           "...and a query value does not");
        /* The literal is SPLIT deliberately. `"\xc3\xb1a"` does not mean what it
         * looks like: a C++ hex escape consumes as many hex digits as it can, so
         * `\xb1a` is one escape (0xB1A, truncated to 0x1A) rather than 0xB1
         * followed by 'a'. The first draft of this test asserted against a
         * mangled input and reported a bug in correct code. Octal escapes are
         * fixed at three digits and do not have the problem, which is why the
         * second half uses them. */
        eq(uri_encode("Campa\xc3\xb1" "a", false), "Campa%C3%B1a",
           "UTF-8 is percent-encoded byte by byte, in UPPERCASE hex");
        eq(uri_encode("ni\303\261os", false), "ni%C3%B1os",
           "...for real Spanish content, which this database is full of");
    }

    std::printf("aws: the signing key chain\n");
    {
        const std::string k =
            signing_key("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", "20150830",
                        "us-east-1", "iam");
        ok(k.size() == 32, "the derived key is 32 bytes");
        // each step must actually narrow: change one input, get a different key
        ok(k != signing_key("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", "20150831",
                            "us-east-1", "iam"),
           "a different DAY gives a different key");
        ok(k != signing_key("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", "20150830",
                            "us-west-2", "iam"),
           "a different REGION gives a different key");
        ok(k != signing_key("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", "20150830",
                            "us-east-1", "s3"),
           "a different SERVICE gives a different key");
    }

    std::printf("aws: the canonical request, field for field\n");
    {
        Credentials c{"AKIDEXAMPLE", "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", ""};
        Request r;
        r.method = "PUT";
        r.host = "example-bucket.s3.us-east-1.amazonaws.com";
        r.path = "/index/directory.json";
        r.payload = "{}";
        r.region = "us-east-1";
        r.service = "s3";
        r.amz_date = "20150830T123600Z";
        const Signed s = sign(r, c);

        const std::string want_canon =
            "PUT\n"
            "/index/directory.json\n"
            "\n"
            "host:example-bucket.s3.us-east-1.amazonaws.com\n"
            "x-amz-content-sha256:" + sha256_hex("{}") + "\n"
            "x-amz-date:20150830T123600Z\n"
            "\n"
            "host;x-amz-content-sha256;x-amz-date\n" +
            sha256_hex("{}");
        eq(s.canonical_request, want_canon,
           "canonical request: method, path, query, headers, signed, hash");

        const std::string want_sts =
            "AWS4-HMAC-SHA256\n"
            "20150830T123600Z\n"
            "20150830/us-east-1/s3/aws4_request\n" +
            sha256_hex(want_canon);
        eq(s.string_to_sign, want_sts, "string to sign: algorithm, date, scope, hash");

        ok(s.signature.size() == 64, "the signature is 64 hex characters");
        ok(s.authorization.find("Credential=AKIDEXAMPLE/20150830/us-east-1/s3/"
                                "aws4_request") != std::string::npos,
           "the Authorization header carries the scoped credential");
        ok(s.authorization.find("SignedHeaders=host;x-amz-content-sha256;"
                                "x-amz-date") != std::string::npos,
           "...and the signed header list, sorted");
        ok(s.headers.count("x-amz-content-sha256") == 1,
           "x-amz-content-sha256 is sent as well as signed (S3 requires it)");
    }

    std::printf("aws: headers are lower-cased, trimmed and sorted\n");
    {
        Credentials c{"AKID", "SECRET", ""};
        Request r;
        r.host = "b.s3.us-east-1.amazonaws.com";
        r.amz_date = "20150830T123600Z";
        r.headers["Content-Type"] = "  application/json  ";
        const Signed s = sign(r, c);
        ok(s.canonical_request.find("content-type:application/json\n") !=
               std::string::npos,
           "a header name is lower-cased and its value trimmed");
        // sorted: content-type < host < x-amz-content-sha256 < x-amz-date
        ok(s.authorization.find("SignedHeaders=content-type;host;"
                                "x-amz-content-sha256;x-amz-date") !=
               std::string::npos,
           "...and the signed list is in sorted order");
    }

    std::printf("aws: a session token is signed when present\n");
    {
        Request r;
        r.host = "b.s3.us-east-1.amazonaws.com";
        r.amz_date = "20150830T123600Z";
        const Signed a = sign(r, Credentials{"AKID", "SECRET", ""});
        const Signed b = sign(r, Credentials{"AKID", "SECRET", "TEMPTOKEN"});
        ok(a.authorization.find("x-amz-security-token") == std::string::npos,
           "absent for long-lived credentials");
        ok(b.authorization.find("x-amz-security-token") != std::string::npos,
           "...and signed for temporary ones (not merely sent)");
        ok(a.signature != b.signature, "...so it changes the signature");
    }

    std::printf("aws: sensitivity — every input reaches the signature\n");
    {
        Credentials c{"AKID", "SECRET", ""};
        Request base;
        base.method = "PUT";
        base.host = "b.s3.us-east-1.amazonaws.com";
        base.path = "/k";
        base.payload = "body";
        base.amz_date = "20150830T123600Z";
        const std::string sig = sign(base, c).signature;

        Request r = base; r.payload = "bodz";
        ok(sign(r, c).signature != sig, "one byte of the BODY changes it");
        r = base; r.path = "/l";
        ok(sign(r, c).signature != sig, "the KEY changes it");
        r = base; r.method = "GET";
        ok(sign(r, c).signature != sig, "the METHOD changes it");
        r = base; r.amz_date = "20150830T123601Z";
        ok(sign(r, c).signature != sig, "one second of the CLOCK changes it");
        r = base; r.region = "us-west-2";
        ok(sign(r, c).signature != sig, "the REGION changes it");
        r = base; r.query["x"] = "1";
        ok(sign(r, c).signature != sig, "a QUERY parameter changes it");
        ok(sign(base, Credentials{"AKID", "SECRE7", ""}).signature != sig,
           "one byte of the SECRET changes it");
        ok(sign(base, c).signature == sig, "and signing is deterministic");
    }

    std::printf(failures ? "\nFAILED (%d)\n"
                         : "\nOK - signing is self-consistent and its primitives"
                           " match published vectors.\n"
                           "     Whether AWS agrees is the operator's first"
                           " call to establish.\n",
                failures);
    return failures ? 1 : 0;
}
