# PRIV OS LAB — Sileo repository

**Experimental rootless iOS package for iPhone SE 2020, iOS 17.5 and Dopamine.**

## Installation through Sileo
Use this HTTPS address in Sileo > Sources > + once GitHub Pages is enabled and its content verified:

`https://r1lad-ui.github.io/privos-sileo/`

Install **PRIV OS LAB 0.5.3 (icônes et fonds)** from Sileo, then open LumenBoard and apply **PRIV OS 0.5**. [Activation et fonds d’écran](ICONES.md).

## Important limitations
- Prototype package: **not yet validated on the target device** for the Control Center modules.
- No VPN included, no guaranteed automatic camera/microphone blocking, and no replacement of iOS.
- The icons are resources that still require a compatible theme engine to activate.
- Two Control Center modules require CCSupport, and should be enabled one by one only after checking stability.
- No automatic modification of SpringBoard on installation.
- This repository's Release metadata are **unsigned**. Never use it for sensitive/commercial security deployments without further testing, signing and audit.

## Maintainer
Experimental PRIV OS project. iOS and Dopamine are third-party software. No affiliation with Apple or GrapheneOS.

## Source directe de test (sans GitHub Pages)
Si GitHub Pages n'est pas encore activé, Sileo peut tenter l'adresse APT brute `https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/`. Les fichiers `Release`, `Packages`, `Packages.gz` et le paquet `.deb` y sont publiés, mais ce point d'accès n'a pas été testé avec Sileo sur l'iPhone.

Pour une adresse permanente adaptée aux utilisateurs, activer dans GitHub **Settings > Pages > Deploy from a branch > main > / (root)**, puis utiliser `https://r1lad-ui.github.io/privos-sileo/` une fois le site réellement accessible.

## Correction publiée le 27 octobre 2026
Version **0.5.1** : les fichiers `Info.plist` des modules utilisaient une clé de classe principale incorrecte. Mettre à jour depuis Sileo, effectuer un seul respring, puis vérifier leur apparition dans Réglages → Centre de contrôle. Les modules sont encore expérimentaux et ne doivent pas être considérés comme testés sur l'appareil.

## Mise à jour 0.5.2 (27 octobre 2026)

- Module **PRIV Control** reconstruit avec le lanceur Apple `CCUIAppLauncherModule` documenté par CCSupport.
- Module **Effacer texte copié** recompilé comme classe Objective-C native `CCUIToggleModule`.
- Icônes et métadonnées des bundles vérifiées ; module 1×1.
- Le code exécutable du coffre PRIV Control est inchangé (empreinte vérifiée).
- **Test sur iPhone requis** : le dépôt et les fichiers sont validés, mais l'affichage et les actions du Centre de contrôle ne sont pas encore confirmés sur l'appareil.
- Aucun script d'installation ne force un respring, ni ne modifie les permissions caméra/micro ou le VPN.

[Voir les notes techniques](CHANGELOG_0.5.2.md).

## Mise à jour 0.5.3 (28 octobre 2026)

- **53 applications couvertes** : 32 nouvelles associations et Calendrier corrigé, avec les icônes précédentes conservées.
- Fond **PRIV Nuit** disponible au téléchargement et dans le paquet ; fond classique conservé.
- Sources SVG et scripts de rendu/construction ajoutés pour les futures extensions.
- L’application native, les modules et les scripts d’installation sont conservés octet pour octet par rapport à la 0.5.2.
- Mise à jour visuelle seulement. Les protections de verrouillage se configurent dans iOS et dans les applications.

[Installation et liste des apps](ICONES.md) · [Détails et reconstruction](CHANGELOG_0.5.3.md) · [Aperçu](previews/PRIV-Icons-0.5.3.png)
