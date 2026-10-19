# Security policy

## Supported version

Until stable releases are published, only the latest commit on the default
branch receives security fixes.

## Reporting a vulnerability

Do not open a public issue for vulnerabilities involving Magpie data,
BLE pairing, command execution, or privacy.

Use GitHub's private vulnerability reporting for this repository. Include
affected versions, reproduction steps, impact, and any suggested mitigation.

The device protocol intentionally accepts only allowlisted semantic actions.
Reports that demonstrate arbitrary command execution, unauthenticated BLE
writes, or disclosure of Magpie data are especially important.
