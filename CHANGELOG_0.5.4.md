# PRIV OS LAB 0.5.4 — Fonds dans Photos

Les fonds du paquet 0.5.3 étaient installés dans le système de fichiers, mais difficiles à trouver depuis l’iPhone. Cette mise à jour ajoute leur accès dans l’application native **PRIV Control**.

## Comportement

- Bouton **Fonds d’écran** visible en haut de l’accueil.
- Galerie des deux images intégrées : **PRIV Nuit** et **PRIV Classique**.
- **Enregistrer dans Photos** écrit uniquement le fond choisi, après une action de l’utilisateur et l’autorisation iOS.
- Affichage de la réussite uniquement après le callback de fin d’enregistrement d’iOS ; refus, restriction et erreur restent visibles avec possibilité de réessayer.
- Un appui répété est bloqué tant qu’un enregistrement est en cours. Après un export réussi, **Enregistrer à nouveau** permet de créer une nouvelle copie.
- Boutons pour ouvrir Photos et les réglages iOS de l’app.

Les images ne sont pas installées automatiquement dans Photos par le script Sileo. La mise à jour n’ajoute pas de catégorie au catalogue Apple des fonds d’écran et ne choisit pas le fond à la place de l’utilisateur.

## Périmètre technique

L’exécutable PRIV Control est reconstruit à partir des sources natives historiques de la version 0.5. Le code de la galerie est isolé dans `native/WallpaperGallery.inc`. Les sources de référence sont conservées dans `native/baseline/`.

Le builder exige que le code du coffre, du chiffrement, de l’import de photo privée et du verrouillage à la perte d’activité reste identique aux sources historiques. Le fichier existant `/var/mobile/Library/PRIVControl/vault.pv4` et son format ne sont pas modifiés par l’installation. Cette vérification de conservation n’est pas un audit de sécurité du coffre.

La galerie utilise `UIImageWriteToSavedPhotosAlbum` et `NSPhotoLibraryAddUsageDescription`. Elle consulte le statut PhotoKit pour le seul niveau `PHAccessLevelAddOnly` et n’énumère pas la photothèque. Les marqueurs locaux d’export sont liés aux empreintes des images et écrits seulement après succès ; ils ne prouvent pas que la copie est toujours présente dans Photos.

Les modules du Centre de contrôle, les 53 associations d’icônes LumenBoard, les scripts d’installation et de désinstallation ainsi que les autres fichiers du paquet 0.5.3 sont conservés octet pour octet. Les seuls fichiers existants modifiés sont l’exécutable de l’app, son `Info.plist` et les métadonnées du paquet. Deux copies des fonds sont ajoutées au bundle natif avec le guide d’utilisation dans les ressources.

## Reconstruction et validation

Voir [native/README.md](native/README.md) pour la compilation et [FONDS.md](FONDS.md) pour l’utilisation.

Les contrôles automatisés portent sur le source préservé, le binaire arm64/iOS, la signature ad hoc, les images, les différences du paquet et les empreintes du dépôt. Un harness C exécute les branches d’enregistrement et de callback avec un runtime simulé : il ne remplace pas UIKit ni les autorisations réelles d’iOS.

Les **19 scénarios du harness passent**. Deux compilations indépendantes produisent le même exécutable, sans avertissement ; ses 25 pages de signature sont vérifiées. Deux constructions du paquet produisent les mêmes octets.

- Paquet : `debs/PRIV_OS_0_5_4_Sileo_iphoneos-arm64.deb`, **2 279 056 octets**.
- SHA-256 du paquet : `2e8f29fbf697a03203bb675afd36db26ac09cb367358ad435c3dde79eee88737`.
- SHA-256 de l’exécutable : `465263022ea8704c91a2514235b9bc04804dab00dcf3b37c072dd6a0db07b21c`.

**Aucun essai sur l’iPhone n’a été effectué depuis cet environnement.** L’ouverture de l’app, la demande Photos et l’application finale du fond restent à vérifier sur l’appareil.
