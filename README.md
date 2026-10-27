# PRIV OS LAB — Sileo repository

**Experimental rootless iOS package for iPhone SE 2020, iOS 17.5 and Dopamine.**

## Installation through Sileo
Use this HTTPS address in Sileo > Sources > + once GitHub Pages is enabled and its content verified:

`https://r1lad-ui.github.io/privos-sileo/`

Install **PRIV OS LAB 0.5** from Sileo. The rootless architecture is `iphoneos-arm64`.

## Important limitations
- Prototype package: **not yet validated on the target device** for the Control Center modules.
- No VPN included, no guaranteed automatic camera/microphone blocking, and no replacement of iOS.
- The icons are resources that still require a compatible theme engine to activate.
- Two Control Center modules require CCSupport, and should be enabled one by one only after checking stability.
- No automatic modification of SpringBoard on installation.
- This repository's Release metadata are **unsigned**. Never use it for sensitive/commercial security deployments without further testing, signing and audit.

## Maintainer
Experimental PRIV OS project. iOS and Dopamine are third-party software. No affiliation with Apple or GrapheneOS.
