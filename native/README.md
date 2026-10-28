# Sources natives de PRIV Control

`PRIVControl.c` est le source C de l’application iOS, utilisant le runtime Objective-C pour UIKit. `WallpaperGallery.inc` ajoute la galerie de fonds et l’export explicite vers Photos dans la version 0.5.4.

`baseline/PRIVControl-0.5.c` provient de l’archive historique du projet `PRIV_OS_0_5_Sources.zip`. Son empreinte SHA-256 est `e6cd6a0c3eed3bf5d0df3d52b1c8f15ba1fa544c7b6b44bc9a52630ddb91c953`. Il sert de référence immuable pour vérifier la conservation du coffre, de son chiffrement, de l’import et du verrouillage automatique.

`sysroot/` contient les cinq stubs `.tbd` minimaux issus de cette même archive. Ce sont des déclarations de symboles pour l’éditeur de liens, pas un SDK Apple complet et pas des bibliothèques exécutables. UIKit, Foundation, Photos et CommonCrypto sont fournis par l’iPhone au moment de l’exécution.

## Construire le paquet 0.5.4

Prérequis de l’hôte Linux : Python 3, Pillow, dpkg-deb, Clang 18 et `ld64.lld`. La toolchain utilisée pour la publication est LLVM **18.1.3** provenant de l’archive Ubuntu ; ses paquets et empreintes figurent dans `toolchain-18.json`.

Avec les paquets LLVM décompressés dans un préfixe local :

```sh
python3 scripts/build_wallpaper_release.py --toolchain /chemin/vers/le/prefixe
```

Avec une toolchain déjà disponible dans `PATH` :

```sh
python3 scripts/build_wallpaper_release.py
```

Les variables `PRIV_CLANG` et `PRIV_LD64` peuvent indiquer les exécutables. Le builder utilise la cible `arm64-apple-ios15.0`, le niveau SDK 17.5, les fixups chaînés et une signature ad hoc générée par l’éditeur de liens. Les modules du Centre de contrôle ne sont pas reconstruits.

Le script repart du paquet 0.5.3 dont l’empreinte est imposée, compile l’app, vérifie le Mach-O, conserve les fichiers non concernés et régénère `Packages`, `Packages.gz` et `Release`. Le rapport détaillé est écrit dans `build/validation-0.5.4.json`.

Pour vérifier séparément un exécutable extrait :

```sh
python3 scripts/verify_native_binary.py /chemin/vers/PRIVControl
```

Pour exécuter les 19 scénarios du module d'export avec les frontières UIKit simulées :

```sh
python3 scripts/test_wallpaper_gallery.py
```

Ce test nécessite un compilateur C sur l'hôte. Il vérifie les refus, le double appui, les callbacks, les réessais et les marqueurs ; il ne simule pas l'autorisation réelle d'iOS ni le rendu de l'écran.

La signature ad hoc et les contrôles statiques ne prouvent pas le fonctionnement sur iPhone et ne constituent pas une validation Apple. Tester sur la cible iPhone SE 2020 avant de considérer le flux Photos comme confirmé.

## Export des fonds

Les deux images sont chargées exclusivement depuis `PRIVControl.app/Wallpapers/`. Aucun fond n’est téléchargé au moment de l’export.

Les appels d’autorisation et d’écriture de la galerie sont limités à l’ajout dans Photos. L’ancien import du coffre utilise toujours `UIImagePickerController` et sa clé `NSPhotoLibraryUsageDescription`. La nouvelle clé `NSPhotoLibraryAddUsageDescription` expose séparément la finalité d’export.

Les boutons de navigation et d’enregistrement sont désactivés pendant une écriture. Le callback UIKit transmet le résultat au thread principal ; les marqueurs `NSUserDefaults` sont écrits uniquement en cas de réussite. L’app ne consulte pas Photos pour détecter une suppression ultérieure : **Enregistrer à nouveau** reste disponible.
