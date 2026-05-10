#pragma once

// DigiCert Global Root G2 — valid until 2038-01-15
// Download the full PEM from:
//   https://www.digicert.com/kb/digicert-root-certificates.htm
// Replace the placeholder below with the complete certificate.
static const char ROOT_CA_CERT[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    // ⚠ PASTE THE FULL DIGICERT GLOBAL ROOT G2 PEM HERE ⚠
    "-----END CERTIFICATE-----\n";
