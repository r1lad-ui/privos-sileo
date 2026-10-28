# Fonds d’écran dans PRIV Control — depuis la 0.5.4

Les fonds **PRIV Nuit** et **PRIV Classique** sont désormais accessibles directement dans l’application. Ils fonctionnent hors ligne et ne nécessitent pas l’ouverture du coffre.

## Installer et enregistrer un fond

1. Dans **Sileo → Sources**, actualiser le dépôt PRIV OS déjà installé.
2. Installer la version actuelle, **PRIV OS LAB 0.5.5 (Sécurité et fonds)**, qui conserve la galerie de la 0.5.4.
3. Fermer PRIV Control depuis le sélecteur d’applications si elle était déjà ouverte, puis la relancer. L’accueil doit indiquer **v0.5.5**.
4. Toucher le bouton vert **Fonds d’écran**, en haut de l’accueil.
5. Sous le fond choisi, toucher **Enregistrer dans Photos**.
6. À la première demande iOS, autoriser l’ajout dans Photos. Attendre le message **Enregistré dans Photos**.
7. Toucher **Ouvrir Photos**, ouvrir l’image ajoutée, puis **Partager → Utiliser en fond d’écran**.

Autre chemin : **Réglages → Fond d’écran → Ajouter un fond d’écran → Photos**. Choisir l’image puis l’appliquer à l’écran verrouillé, à l’accueil ou aux deux selon les options proposées par iOS.

L’installation seule ne remplit pas la photothèque : l’ajout se fait après ton appui et l’autorisation iOS. Cette version fournit des images sélectionnables depuis **Photos**, sans ajouter de catégorie à la galerie Apple ni modifier automatiquement le fond courant.

## En cas de difficulté

- **Le bouton Fonds d’écran n’apparaît pas** : vérifier la version actuelle dans Sileo, fermer complètement PRIV Control puis la rouvrir. La galerie existe depuis la 0.5.4 ; il n’est pas nécessaire de réinstaller le moteur de thème.
- **L’accès est refusé** : toucher **Réglages iOS** dans la galerie pour ouvrir les réglages de PRIV Control, autoriser l’ajout à Photos, puis revenir et réessayer. Une restriction iOS ou Temps d’écran peut devoir être levée séparément.
- **L’image a été supprimée de Photos** : toucher **Enregistrer à nouveau**. L’application mémorise un export réussi, mais ne lit pas la photothèque pour vérifier si cette copie existe encore.
- **Un enregistrement échoue** : aucun succès n’est affiché tant qu’iOS ne l’a pas confirmé ; vérifier l’autorisation et l’espace libre, puis réessayer.

L’enregistrement utilise l’autorisation **d’ajout uniquement**. L’ancienne fonction d’import d’une photo dans le coffre conserve sa propre description d’autorisation et son fonctionnement.

## Liens directs facultatifs

- [PRIV Nuit](https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/wallpapers/PRIV-Wallpaper-Night.png)
- [PRIV Classique](https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/wallpapers/PRIV-Wallpaper-Classic.png)

Le dépôt Sileo utilisé pour ce projet est `https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/`.

Références Apple : [changer le fond d’écran](https://support.apple.com/fr-fr/102638), [autorisation d’ajout dans Photos](https://developer.apple.com/documentation/bundleresources/information-property-list/nsphotolibraryaddusagedescription).
