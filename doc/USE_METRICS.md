# Obtain Momo statistics

## Overview

Momo includes a built-in MetricsServer that can retrieve statistics in JSON format via an HTTP API. This section explains how to start MetricsServer and the response format.

## How to Start MetricsServer

MetricsServer is not started by default. To start it, specify the port number with the `--metrics-port` option. To start it on port 8081, use the following command:

`bash
./momo --metrics-port 8081 <mode>
``

By default, MetricsServer is only accessible from the loopback address (listening on 127.0.0.1). To allow global access (listening on 0.0.0.0), specify the `--metrics-allow-external-ip` argument.

## Statistics API

### URL

To retrieve statistics, use the HTTP GET method to access `/metrics`. For example, if you start Momo with the following options:

```bash
./momo --metrics-port 8081 p2p
```

You can retrieve statistics by accessing <http://127.0.0.1:8081/metrics> in a browser or with an HTTP client such as `curl`.

### Statistics JSON Specification

Statistics can be retrieved in JSON format. The JSON contains the following information:

```json
{
"version": "Return value of MomoVersion::GetClientName()",
"environment": "Return value of MomoVersion::GetEnvironmentName()",
"libwebrtc": "Return value of MomoVersion::GetLibwebrtcName()",
"stats": [`werbrtc::RTCStats`, ...] // Same as those included in the pong message in Sora mode"
}
```

An example of an actual response looks like this:

```json
{
"version": "WebRTC Native Client Momo 2020.11 (39607520)",
"environment": "[aarch64] Ubuntu 18.04.5 LTS (nvidia-l4t-core 32.4.4-20201016123640)",
"libwebrtc": "Shiguredo-Build M88.4324@{#3} (88.4324.3.0 b15b2915)", 
"stats": [ 
{ 
"base64Certificate": "MIIBFjCBvaADAgECAgkApU7zxF5fhbcwCgYIKoZIzj0EAwIwETEPMA0GA1UEAwwGV2ViUlRDMB4XDTIwMTIxODA1Mjc0OV oXDTIxMDExODA1Mjc0OVowETEPMA0GA1UEAwwGV2ViUlRDMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEDwRrT2pCTn8ST tVcaG0Jy78ZGcW2Kl+DjvJyveMA/4avzcHRtmAfB8R0197uxvFPDPE+MVwAp3xJjfxPnbNeajAKBggqhkjOPQQDAgNIADB FAiBPKd9HcryWjjL9mhdnqeHbvdjLa/aDzT9OVNChJ0tN9QIhANayX7S64nedOQPG5COZohnAicvNMzhpuJMgxHXhSByy", 
"fingerprint": "39:BD:A1:C5:40:EB:D4:DA:40:7D:29:E5:DC:70:21:F0:C7:0C:1D:2C:4C:CE:38:C5:11:CB:0F:AD:6D:A0:B1:DF", 
"fingerprintAlgorithm": "sha-256", 
"id": "RTCCertificate_39:BD:A1:C5:40:EB:D4:DA:40:7D:29:E5:DC:70:21:F0:C7:0C:1D:2C:4C:CE:38:C5:11:CB:0F:AD:6D:A0:B1:DF", 
"timestamp": 1608355682366521, 
"type": "certificate" 
}, 
{ 
"base64Certificate": "MIIBFTCBvaADAgECAgkAijBOQg68QKUwCgYIKoZIzj0EAwIwETEPMA0GA1UEAwwGV2ViUlRDMB4XDTIwMTIxODA1MjczNV oXDTIxMDExODA1MjczNVowETEPMA0GA1UEAwwGV2ViUlRDMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEK9br1cWzDzvMn P+0d6e/RvPgFdwvMt5pqm2RHHweOCsJJqWthSk7S3l5Yve48cDqED0vQ6JPQXdlaVP+MqZuBTAKBggqhkjOPQQDAgNHADB EAiA83BHLPl1tUJDXykYaCBhegWVfr4rUqwP3JPY/99ZZEQIgY1Iik0NrGENgM+nxLbR1/W4M52khQ6rB5CS6PN63mtM=", 
"fingerprint": "D5:76:46:FA:F4:9A:CD:6F:95:0A:A1:35:4D:FC:D1:62:89:0B:F8:B7:B3:91:EE:A0:35:78:1A:04:B7:1F:75:3E", 
"fingerprintAlgorithm": "sha-256", 
"id": "RTCCertificate_D5:76:46:FA:F4:9A:CD:6F:95:0A:A1:35:4D:FC:D1:62:89:0B:F8:B7:B3:91:EE:A0:35:78:1A:04:B7:1F:75:3E", 
"timestamp": 1608355682366521, 
"type": "certificate" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_102", 
"mimeType": "video/H264", 
"payloadType": 102, 
"sdpFmtpLine": "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_107", 
"mimeType": "video/rtx", 
"payloadType": 107, "sdpFmtpLine": "apt=125", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_108", 
"mimeType": "video/H264", 
"payloadType": 108, 
"sdpFmtpLine": "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_109", 
"mimeType": "video/rtx", 
"payloadType": 109, 
"sdpFmtpLine": "apt=108", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_114", 
"mimeType": "video/red", 
"payloadType": 114, 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_115", 
"mimeType": "video/rtx", 
"payloadType": 115, 
"sdpFmtpLine": "apt=114", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_116", 
"mimeType": "video/ulpfec", 
"payloadType": 116, 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_120", 
"mimeType": "video/rtx", 
"payloadType": 120, 
"sdpFmtpLine": "apt=127", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_Inbound_121", 
"mimeType": "video/rtx", 
"payloadType": 121, 
"sdpFmtpLine": "apt=102", 
"timestamp": 1608355682366521, 
"type": "codec" 
}, 
{ 
"clockRate": 90000, 
"id": "RTCCodec_0_In