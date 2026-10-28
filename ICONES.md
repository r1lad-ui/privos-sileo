# Icônes et fonds PRIV OS — version 0.5.4

La mise à jour **PRIV OS LAB 0.5.4 (Fonds dans Photos)** conserve le thème enrichi en 0.5.3, déjà utilisé avec **LumenBoard**, et rend les fonds accessibles dans l'app. Elle contient **53 identifiants d'applications**, dont PRIV Control. Le nom du thème reste **PRIV OS 0.5**, afin de conserver la sélection existante.

## Mettre à jour les icônes

1. Dans **Sileo → Sources**, actualiser le dépôt PRIV OS déjà présent.
2. Installer la mise à jour **PRIV OS LAB 0.5.4**.
3. Ouvrir **LumenBoard**, sélectionner le thème **PRIV OS 0.5**, puis **Apply**.
4. Si quelques anciennes icônes persistent, utiliser une fois **LumenBoard → Settings → Clear icon cache & respring**, puis vérifier le résultat.

LumenBoard fonctionne déjà sur cet appareil : aucune réinstallation du moteur n'est nécessaire. Ne pas réinstaller SnowBoard, qui avait provoqué un Safe Mode sur cette configuration, et ne pas cumuler plusieurs moteurs de thème.

Le répertoire reste `/var/jb/Library/Themes/PRIV OS 0.5.theme/`. Les PNG de 512 × 512 pixels sont associés aux identifiants exacts dans `IconBundles/`, avec des noms français et anglais de secours dans `Icons/`.

## Applications complétées

- Telegram, Signal, Proton VPN, Rappels, Calendrier, Podcasts, Revolut, WhatsApp, Exodus, Crypto.com, Crypto.com Onchain / DeFi, Alibaba.com et Contacts.
- Proton Mail, Proton Pass, Brave, Firefox, Bitwarden, Google Authenticator, Microsoft Authenticator, YouTube, Spotify, ChatGPT et AliExpress.
- Sileo, Dopamine, Filza, Dictaphone, Livres, Traduire, Raccourcis, Bourse et Freeform.

Les 20 autres icônes de la version précédente sont conservées. Seul le **Calendrier** est remplacé par un pictogramme neutre : l'ancien affichait un « 27 » figé. Cette version ne promet pas de date dynamique. L'ancien Crypto.com Onchain / DeFi (`com.defi.wallet`) peut désormais être nommé **Ult** selon la version installée ; il reste distinct de l'app Crypto.com principale.

Le thème ne télécharge pas ces applications. Une application absente du thème conserve son icône d'origine. Certaines applications proposent plusieurs versions ou icônes alternatives : leur résultat doit être vérifié sur l'iPhone.

## Appliquer le fond d'écran

Après la mise à jour, fermer complètement **PRIV Control**, la rouvrir et toucher **Fonds d'écran → Enregistrer dans Photos**. Autoriser l'ajout si iOS le demande, attendre la confirmation, puis toucher **Ouvrir Photos**. Ouvrir l'image et choisir **Partager → Utiliser en fond d'écran**. [Guide et dépannage](FONDS.md).

Les fonds peuvent aussi être ouverts directement depuis le dépôt :

- [Fond PRIV Nuit](https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/wallpapers/PRIV-Wallpaper-Night.png)
- [Fond PRIV Classique](https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/wallpapers/PRIV-Wallpaper-Classic.png)

Sur l'iPhone, ouvrir le fond dans Safari et l'enregistrer dans Photos, puis aller dans **Réglages → Fond d'écran → Ajouter un fond d'écran → Photos**. Sélectionner l'image et choisir où l'appliquer. iOS conserve le choix final de l'utilisateur ; l'installation du paquet ne change pas automatiquement son fond.

Les deux fichiers sont aussi installés dans `/var/jb/usr/share/privos/wallpapers/`, accessibles avec Filza. Le fond historique `/var/jb/usr/share/privos/PRIV-Wallpaper.png` reste disponible.

## Validation et limites

Les fichiers PNG, les identifiants, l'intégrité du paquet et les sommes de contrôle du dépôt sont vérifiés avant publication. La version 0.5.4 reconstruit PRIV Control pour ajouter la galerie ; le code du coffre, de l'import privé et du verrouillage est conservé. Le thème, les modules du Centre de contrôle et les scripts d'installation de la 0.5.3 restent identiques. Le rendu sur l'iPhone et l'autorisation Photos restent à confirmer sur l'appareil.

Sources techniques : [LumenBoard](https://github.com/xsxs18-dev/LumenBoard), [identifiants Apple](https://support.apple.com/guide/deployment/bundle-ids-for-iphone-and-ipad-apple-apps-depece748c41/web). Le relevé des identifiants et des sources est conservé dans `assets/icons/verified-identifiers.json`.
