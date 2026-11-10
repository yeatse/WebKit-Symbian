#!/usr/bin/env python3
"""Positive local IPv6/HTTPS/WSS and bounded HTTP/2 integration acceptance."""
import argparse
import asyncio
import json
from pathlib import Path
import socket
import ssl
import subprocess

import h2.config
import h2.connection
import h2.events
import h2.settings


async def run(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cert, key = out / "localhost-cert.pem", out / "localhost-key.pem"
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
        "-keyout", str(key), "-out", str(cert), "-subj", "/CN=localhost",
        "-addext", "subjectAltName=DNS:localhost,DNS:fallback.test,IP:::1,IP:127.0.0.1",
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    context1 = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context1.load_cert_chain(cert, key)
    context1.set_alpn_protocols(["http/1.1"])
    context2 = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context2.load_cert_chain(cert, key)
    context2.set_alpn_protocols(["h2", "http/1.1"])
    report = {"http1_requests": 0, "wss_echoes": 0, "h2_connections": 0,
              "h2_requests": 0, "h2_peak_active_streams": 0,
              "h2_stream_ids": [], "h2_total_bytes": 0, "window_updates": 0,
              "client_settings": {}, "errors": []}

    async def http1(reader, writer):
        try:
            request = await reader.readuntil(b"\r\n\r\n")
            if b"Upgrade: websocket\r\n" in request:
                writer.write(b"HTTP/1.1 101 Switching Protocols\r\n"
                             b"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                             b"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")
                await writer.drain()
                frame = await reader.readexactly(10)
                assert frame[:2] == b"\x81\x84"
                mask = frame[2:6]
                message = bytes(v ^ mask[i % 4] for i, v in enumerate(frame[6:]))
                assert message == b"ping"
                writer.write(b"\x81\x04" + message)
                report["wss_echoes"] += 1
            else:
                writer.write(b"HTTP/1.1 200 OK\r\nContent-Length: 16\r\n"
                             b"Connection: close\r\n\r\n" + b"x" * 16)
                report["http1_requests"] += 1
            await writer.drain()
        except Exception as exc:
            report["errors"].append(f"http1: {exc!r}")
        finally:
            writer.close()

    async def http2(reader, writer):
        connection = h2.connection.H2Connection(
            config=h2.config.H2Configuration(client_side=False))
        connection.initiate_connection()
        writer.write(connection.data_to_send())
        report["h2_connections"] += 1
        active = set()
        tasks = []
        lengths = {}
        window_changed = asyncio.Event()

        async def respond(stream):
            try:
                # Hold normal responses briefly so concurrent requests overlap.
                await asyncio.sleep(0.1)
                remaining = lengths[stream]
                connection.send_headers(stream, [(":status", "200"), ("content-length", str(remaining))])
                while remaining:
                    size = min(remaining, connection.local_flow_control_window(stream),
                               connection.max_outbound_frame_size)
                    if not size:
                        window_changed.clear()
                        await window_changed.wait()
                        continue
                    remaining -= size
                    connection.send_data(stream, b"x" * size, end_stream=not remaining)
                    report["h2_total_bytes"] += size
                    writer.write(connection.data_to_send())
                    await writer.drain()
                active.remove(stream)
            except Exception as exc:
                report["errors"].append(f"http2 response: {exc!r}")

        try:
            assert writer.get_extra_info("ssl_object").selected_alpn_protocol() == "h2"
            while data := await reader.read(65536):
                for event in connection.receive_data(data):
                    if isinstance(event, h2.events.RemoteSettingsChanged):
                        for setting, change in event.changed_settings.items():
                            report["client_settings"][str(int(setting))] = change.new_value
                    elif isinstance(event, h2.events.RequestReceived):
                        active.add(event.stream_id)
                        path = dict(event.headers)[b":path"]
                        lengths[event.stream_id] = 512 * 1024 if path == b"/item/0" else 16384
                        report["h2_requests"] += 1
                        report["h2_stream_ids"].append(event.stream_id)
                        report["h2_peak_active_streams"] = max(
                            report["h2_peak_active_streams"], len(active))
                    elif isinstance(event, h2.events.StreamEnded):
                        tasks.append(asyncio.create_task(respond(event.stream_id)))
                    elif isinstance(event, h2.events.WindowUpdated):
                        report["window_updates"] += 1
                        window_changed.set()
                writer.write(connection.data_to_send())
                await writer.drain()
            await asyncio.gather(*tasks)
        except (ConnectionResetError, BrokenPipeError):
            pass
        except Exception as exc:
            report["errors"].append(f"http2: {exc!r}")
        finally:
            writer.close()

    async def listen(family, address, handler, context):
        sock = socket.socket(family, socket.SOCK_STREAM)
        if family == socket.AF_INET6:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        sock.bind((address, 0))
        sock.listen()
        sock.setblocking(False)
        server = await asyncio.start_server(handler, sock=sock, ssl=context)
        return server, sock.getsockname()[1]

    server6, port6 = await listen(socket.AF_INET6, "::1", http1, context1)
    server4, port4 = await listen(socket.AF_INET, "127.0.0.1", http1, context1)
    server2, port2 = await listen(socket.AF_INET6, "::1", http2, context2)
    process = None
    try:
        process = await asyncio.create_subprocess_exec(
            str(args.client.resolve()), str(cert), str(port6), str(port4), str(port2),
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
        stdout, _ = await asyncio.wait_for(process.communicate(), 45)
        output = stdout.decode()
        print(output, end="")
        (out / "client.log").write_text(output)
        await asyncio.sleep(0.05)
        report["client_exit"] = process.returncode
        assert not process.returncode, report
        assert not report["errors"], report
        assert report["http1_requests"] == 3 and report["wss_echoes"] == 1, report
        assert report["h2_requests"] == 12 and report["h2_connections"] == 1, report
        assert len(set(report["h2_stream_ids"])) == 12, report
        assert report["h2_peak_active_streams"] == 8, report
        assert report["h2_total_bytes"] == 512 * 1024 + 11 * 16384, report
        assert report["window_updates"] > 0, report
        assert report["client_settings"]["3"] == 8, report
        assert report["client_settings"]["4"] == 256 * 1024, report
        assert report["client_settings"]["2"] == 0, report
        report["result"] = "PASS"
    finally:
        if process and process.returncode is None:
            process.kill()
            await process.wait()
        for server in (server6, server4, server2):
            server.close()
            await server.wait_closed()
        # The short-lived test key is unnecessary in retained evidence.
        key.unlink(missing_ok=True)
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    parser.add_argument("output", type=Path)
    asyncio.run(run(parser.parse_args()))
