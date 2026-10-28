# PRIV Key LAB 0.1.1

Application iOS native de relevé manuel de silhouette, pour l'iPhone SE 2020 sous iOS 17.5 et les jailbreaks rootless compatibles. Aucun site ni WebView dans l'application. Elle utilise UIKit, les sélecteurs de photos d'iOS et SceneKit.

## Installation

Le paquet s'appelle **PRIV Key LAB**, identifiant fr.privos.keylab. Il est distinct de PRIV OS LAB et utilise la même source Sileo :

    https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/

Après la réussite du workflow **Build PRIV Key**, actualiser les sources dans Sileo, rechercher **PRIV Key LAB**, puis installer. L'icône **PRIV Key** est enregistrée par le script du paquet. Le paquet installe seulement son application dans /var/jb/Applications/PRIVKey.app. Il ne modifie ni PRIV Control, ni ses données, ni SpringBoard.

Le fichier installable est publié dans debs/fr.privos.keylab_0.1.1_iphoneos-arm64.deb. La signature ad hoc de ce paquet nécessite un environnement jailbreaké compatible.

### Correction d'installation 0.1.1

La version 0.1.0 omettait les entrées de dossiers dans l'archive du paquet. Une installation neuve avec dpkg pouvait échouer avec « unable to create … PRIVKey.app/… : No such file or directory ». La version 0.1.1 déclare les dossiers avant leurs fichiers et utilise le shell rootless /var/jb/bin/sh dans les scripts d'installation et de retrait.

Si l'installation 0.1.0 a échoué, actualiser la source dans Sileo et installer la version 0.1.1. Il n'est pas nécessaire de supprimer les relevés ni de réinitialiser le jailbreak pour essayer cette mise à jour. Si une erreur demeure, conserver les lignes du journal situées avant « subprocess returned an error code » : cette dernière ligne seule ne précise pas la cause.

## Premier essai

1. Ouvrir l'application : un **exemple fictif**, clairement identifié, est déjà chargé.
2. Ouvrir **Voir en 3D**, faire tourner l'objet et revenir.
3. Ouvrir **Exporter le relevé**, choisir STL, SVG ou JSON puis Enregistrer dans Fichiers.
4. Enregistrer le relevé dans PRIV Key ; le retrouver avec le bouton dossier.
5. Importer le JSON depuis Fichiers pour vérifier la reprise du relevé.

Ces essais vérifient le fonctionnement de l'application et des exports ; ils ne valident aucune clé fonctionnelle.

## Relevé d'une clé de test

La première version traite une silhouette manuelle à épaisseur constante. Utiliser une clé possédée ou confiée pour le travail autorisé.

- Importer une photo de face ou utiliser la caméra. Une référence de longueur doit figurer dans le même plan que la clé.
- Dans **Échelle**, placer les repères A et B, saisir leur distance réelle en millimètres, puis appliquer.
- Dans **Contour**, placer les points dans l'ordre autour de la silhouette. Fermer le contour.
- Dans **Ajuster**, déplacer les points au doigt ou saisir leurs coordonnées. Annuler et Refaire sont disponibles.
- Mesurer l'épaisseur et appliquer sa valeur avant la prévisualisation ou l'export.
- Le déplacement de la vue se fait à deux doigts et le zoom par pincement.

La fonction **Échelle écran** permet un étalonnage avec une règle, puis l'affichage du contour selon cette échelle. Un pincement ultérieur quitte cette échelle et revient au zoom libre. Vérifier de nouveau l'étalonnage après un changement d'appareil ou de zoom d'affichage.

## Exports et limites

- **JSON** : relevé modifiable, y compris la photo si elle a été importée.
- **SVG** : contour avec des dimensions explicites en millimètres.
- **STL** : maillage fermé d'une extrusion constante. Les coordonnées sont exprimées en millimètres ; le format STL ne transporte pas lui-même d'unité. Choisir les millimètres dans le logiciel d'impression.

L'étalonnage par deux points ne corrige pas la perspective d'une photo oblique. La silhouette ne reconstruit pas les rainures, profils latéraux, trous ou géométries cachées. La précision du relevé, le matériau, les tolérances et la résistance de la fabrication ne sont pas établis par l'application. Un STL valide ne constitue pas la garantie d'un double utilisable.

Les croisements et les contours dégénérés sont refusés avant l'export. Le maillage est vérifié par des tests automatisés de fermeture, d'orientation et de volume.

## Données

L'application n'emploie aucun service réseau, compte, publicité ou bibliothèque de suivi. La photo est traitée dans l'application. Le relevé reste en mémoire jusqu'à son enregistrement explicite.

Les relevés enregistrés sont conservés dans le dossier Application Support/PRIVKey/Releves de l'utilisateur iOS, avec la protection de fichiers iOS demandée par l'application. Ce n'est pas un coffre chiffré propre à PRIV Key. La sécurité dépend aussi du téléphone et du jailbreak.

Les fichiers temporaires utilisés par le partage sont supprimés après la fermeture de la feuille de partage ; les restes de plus de 24 heures sont nettoyés au lancement. Une copie confiée à Fichiers, AirDrop ou une autre application est ensuite gérée par cette destination.

Le bouton dossier permet de supprimer les relevés enregistrés. Une désinstallation ne doit pas être supposée effacer les relevés : les supprimer explicitement au préalable si nécessaire.

## Construction et vérification

Les sources se trouvent dans native/PRIVKey/. Sur macOS avec Xcode :

    python3 scripts/build_keylab.py

Le workflow .github/workflows/build-keylab.yml compile l'application arm64, vérifie sa signature ad hoc, crée le paquet rootless, puis l'ajoute à la source Sileo en conservant les autres paquets.

Avant la compilation, un test Linux effectue un véritable dépaquetage dpkg dans des racines temporaires isolées, dont une avec /var/jb représenté par un lien symbolique. Les fichiers de test ne sont jamais exécutés et la base de paquets de l'hôte n'est pas modifiée. Ce test vérifie la structure et les permissions nécessaires à l'installation ; il ne remplace pas la configuration ni l'exécution sur iOS.

Les tests de géométrie et d'emballage peuvent aussi être exécutés sur Linux :

    python3 scripts/build_keylab.py --test-only

L'artefact de compilation contient le .deb et le rapport validation.json. Les tests d'hôte et la compilation ne remplacent pas un essai de UIKit, de la caméra, du partage et des mesures physiques sur l'iPhone cible.
