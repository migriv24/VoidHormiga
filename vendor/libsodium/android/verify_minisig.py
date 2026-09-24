"""Verify a minisign signature with the `cryptography` package (no minisign binary).

Format (minisign's own spec): line 2 is base64(alg[2] | key_id[8] | sig[64]);
alg "ED" means the signed message is BLAKE2b-512(file), "Ed" means the file
itself. Line 4 is base64(global_sig[64]) over sig | trusted_comment_text."""
import base64
import hashlib
import sys

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

PUBKEY = "RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3"  # libsodium's, from libsodium.org

pk = base64.b64decode(PUBKEY)
assert pk[:2] == b"Ed"
pk_id, pk_raw = pk[2:10], pk[10:42]
lines = open(sys.argv[2], encoding="utf-8").read().splitlines()
blob = base64.b64decode(lines[1])
alg, key_id, sig = blob[:2], blob[2:10], blob[10:74]
assert key_id == pk_id, "signed by a different key"
data = open(sys.argv[1], "rb").read()
msg = hashlib.blake2b(data, digest_size=64).digest() if alg == b"ED" else data
key = Ed25519PublicKey.from_public_key_bytes(pk_raw) if hasattr(Ed25519PublicKey, "from_public_key_bytes") else Ed25519PublicKey.from_public_bytes(pk_raw)
key.verify(sig, msg)
trusted = lines[2].split("trusted comment: ", 1)[1]
key.verify(base64.b64decode(lines[3]), sig + trusted.encode())
print("GOOD signature by libsodium's key;", trusted)
