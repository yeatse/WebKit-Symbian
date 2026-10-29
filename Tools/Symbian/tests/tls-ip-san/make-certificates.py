#!/usr/bin/env python3
"""Create private, signed certificates for strict IP identity checks; no network."""

import argparse
import datetime
import hashlib
import ipaddress
import json
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    now = datetime.datetime.now(datetime.UTC)
    ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    other_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    leaf_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    issuer = x509.Name(
        [x509.NameAttribute(NameOID.COMMON_NAME, "Private IP SAN regression CA")]
    )
    other_issuer = x509.Name(
        [x509.NameAttribute(NameOID.COMMON_NAME, "Untrusted IP SAN regression CA")]
    )

    def certificate(
        name,
        key,
        signing_key,
        subject,
        issuer_name,
        sans=None,
        start=-1,
        end=2,
        ca=False,
    ):
        builder = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(issuer_name)
            .public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now + datetime.timedelta(days=start))
            .not_valid_after(now + datetime.timedelta(days=end))
            .add_extension(
                x509.BasicConstraints(ca=ca, path_length=None), critical=True
            )
        )
        if sans is not None:
            builder = builder.add_extension(
                x509.SubjectAlternativeName(sans), critical=False
            )
        cert = builder.sign(signing_key, hashes.SHA256())
        (args.output / (name + ".der")).write_bytes(
            cert.public_bytes(serialization.Encoding.DER)
        )
        (args.output / (name + ".pem")).write_bytes(
            cert.public_bytes(serialization.Encoding.PEM)
        )

    certificate("ca", ca_key, ca_key, issuer, issuer, start=-2, end=7, ca=True)
    certificate(
        "other-ca",
        other_key,
        other_key,
        other_issuer,
        other_issuer,
        start=-2,
        end=7,
        ca=True,
    )
    dns = x509.DNSName
    ip = lambda value: x509.IPAddress(ipaddress.ip_address(value))
    variants = {
        "dual": {"sans": [dns("localhost"), ip("127.0.0.1")]},
        "ipv6": {"sans": [ip("::1"), ip("2001:db8::1")]},
        "mapped": {"sans": [ip("::ffff:127.0.0.1")]},
        "dns-numeric": {"sans": [dns("127.0.0.1"), dns("::1")]},
        "cn-numeric": {"sans": None, "cn": "127.0.0.1"},
        "cn-ipv6": {"sans": None, "cn": "::1"},
        "mixed-wrong-ip": {"sans": [dns("127.0.0.1"), ip("127.0.0.2")]},
        "wrong-dns-right-ip": {"sans": [dns("wrong.test"), ip("127.0.0.1")]},
        "wildcard": {"sans": [dns("*.example.test")]},
        "numeric-wildcard": {"sans": [dns("*.0.0.1")]},
        "expired": {"sans": [ip("127.0.0.1")], "start": -3, "end": -2},
        "future": {"sans": [ip("127.0.0.1")], "start": 2, "end": 3},
        "untrusted": {"sans": [ip("127.0.0.1")], "untrusted": True},
    }
    for name, options in variants.items():
        subject = x509.Name(
            [x509.NameAttribute(NameOID.COMMON_NAME, options.get("cn", "localhost"))]
        )
        untrusted = options.get("untrusted", False)
        certificate(
            name,
            leaf_key,
            other_key if untrusted else ca_key,
            subject,
            other_issuer if untrusted else issuer,
            options["sans"],
            options.get("start", -1),
            options.get("end", 2),
        )
    # Expected flags use stable mbedTLS X.509 definitions: mismatch=4,
    # untrusted=8, expired=1, future=512. Negative cases require those bits;
    # unrelated validation errors may never turn a positive case into PASS.
    cases = [
        ("dual", "127.0.0.1", 0),
        ("dual", "localhost", 0),
        ("dual", "127.0.0.2", 4),
        ("dual", "127.0.0.10", 4),
        ("ipv6", "::1", 0),
        ("ipv6", "0:0:0:0:0:0:0:1", 0),
        ("ipv6", "2001:0db8:0:0::0001", 0),
        ("ipv6", "2001:db8::2", 4),
        ("mapped", "::ffff:7f00:1", 0),
        ("mapped", "127.0.0.1", 4),
        ("dns-numeric", "127.0.0.1", 4),
        ("dns-numeric", "::1", 4),
        ("cn-numeric", "127.0.0.1", 4),
        ("cn-ipv6", "::1", 4),
        ("mixed-wrong-ip", "127.0.0.1", 4),
        ("wrong-dns-right-ip", "127.0.0.1", 0),
        ("numeric-wildcard", "127.0.0.1", 4),
        ("wildcard", "www.example.test", 0),
        ("wildcard", "a.b.example.test", 4),
        ("dual", "other.test", 4),
        ("expired", "127.0.0.1", 1),
        ("future", "127.0.0.1", 512),
        ("untrusted", "127.0.0.1", 8),
        ("expired", "127.0.0.2", 1 | 4),
        ("untrusted", "127.0.0.2", 8 | 4),
    ]
    manifest = {
        "created_utc": now.isoformat(),
        "cases": [
            {"certificate": name + ".der", "hostname": host, "required_flags": flags}
            for name, host, flags in cases
        ],
        "files": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(args.output.iterdir())
            if path.is_file()
        },
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Created {len(cases)} certificate identity cases in {args.output}")


if __name__ == "__main__":
    main()
