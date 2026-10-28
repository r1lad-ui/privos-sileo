# PRIV Key LAB 0.2.0

Application iOS native de relevé manuel de silhouette, pour l'iPhone SE 2020 sous iOS 17.5 et les jailbreaks rootless compatibles. Aucun site ni WebView dans l'application. Elle utilise UIKit, les sélecteurs de photos d'iOS et SceneKit.

La version 0.2.0 ajoute des références de taille **carte ID-1 et pièces en euros**, un **axe de mesure orientable**, des **traits déplaçables**, une **loupe pendant le déplacement** et un **éditeur plein écran**. Le fonctionnement s'inspire du principe montré dans la vidéo fournie, avec une implémentation propre à PRIV Key. La vidéo n'est pas incluse dans le paquet.

## Installation

Le paquet s'appelle **PRIV Key LAB**, identifiant fr.privos.keylab. Il est distinct de PRIV OS LAB et utilise la même source Sileo :

    https://raw.githubusercontent.com/r1lad-ui/privos-sileo/main/

Après la réussite du workflow **Build PRIV Key**, actualiser les sources dans Sileo, rechercher **PRIV Key LAB**, puis installer. L'icône **PRIV Key** est enregistrée par le script du paquet. Le paquet installe seulement son application dans /var/jb/Applications/PRIVKey.app. Il ne modifie ni PRIV Control, ni ses données, ni SpringBoard.

Le fichier installable est publié dans debs/fr.privos.keylab_0.2.0_iphoneos-arm64.deb. La signature ad hoc de ce paquet nécessite un environnement jailbreaké compatible.

### Correction d'installation 0.1.1

La version 0.1.0 omettait les entrées de dossiers dans l'archive du paquet. Une installation neuve avec dpkg pouvait échouer avec « unable to create … PRIVKey.app/… : No such file or directory ». La version 0.1.1 déclare les dossiers avant leurs fichiers et utilise le shell rootless /var/jb/bin/sh dans les scripts d'installation et de retrait.

Cette correction est conservée dans la version 0.2.0. Si l'installation 0.1.0 a échoué, actualiser la source dans Sileo et installer la dernière version. Si une erreur demeure, conserver les lignes du journal situées avant « subprocess returned an error code » : cette dernière ligne seule ne précise pas la cause.

## Premier essai

1. Ouvrir l'application : un **exemple fictif**, clairement identifié, est déjà chargé avec trois traits de mesure.
2. Dans **Traits**, sélectionner un point, le déplacer et observer ses coordonnées X/Y. Le dessin fictif est déjà fermé : le rouvrir dans **Contour** pour essayer la copie d'un point.
3. Utiliser le bouton d'agrandissement en haut à droite pour travailler en plein écran ; **Terminé** ramène aux commandes.
4. Ouvrir **Voir en 3D**, faire tourner l'objet et revenir.
5. Enregistrer le relevé ; le retrouver avec le bouton dossier. Exporter puis réimporter un JSON pour vérifier la reprise des traits et de la référence.

Ces essais vérifient le fonctionnement de l'application et des exports ; ils ne valident aucune clé fonctionnelle.

## Photo et référence de taille

La première version traite une silhouette manuelle à épaisseur constante. Utiliser une clé possédée ou confiée pour le travail autorisé.

Importer une photo ou utiliser la caméra. Photographier de face, avec une carte en fond, une pièce à côté ou une règle. La référence et le contour à mesurer doivent se trouver à la même hauteur et aussi près que possible l'un de l'autre. Un écart de hauteur peut changer l'échelle apparente ; une vue oblique déforme les dimensions.

Dans **Référence**, sélectionner l'objet réellement présent :

| Référence | Longueur A–B utilisée |
|---|---:|
| Carte ID-1, grand côté | 85,60 mm |
| Carte ID-1, petit côté | 53,98 mm |
| Pièce de 1 € | diamètre 23,25 mm |
| Pièce de 2 € | diamètre 25,75 mm |
| Pièce de 50 centimes | diamètre 24,25 mm |
| Mesure personnalisée | longueur saisie, de 0,1 à 1 000 mm |

Les dimensions de carte correspondent au format ID-1 ; toutes les cartes ne suivent pas ce format. Source : [OACI, Doc 9303 partie 5, §2.1, citant ISO/IEC 7810:2019](https://www.icao.int/sites/default/files/publications/DocSeries/9303_p5_cons_fr.pdf). Diamètres des pièces : [Banque centrale européenne, caractéristiques des faces communes](https://www.ecb.europa.eu/euro/coins/common/html/index.en.html).

**Carte :** placer A et B aux deux coins du côté choisi. Comme les vrais coins sont arrondis, viser l'intersection imaginaire des prolongements des bords droits. Un rectangle aux proportions ID-1 s'affiche pour vérifier l'alignement. **Inverser la carte** place le rectangle de l'autre côté du segment A–B. Si le rectangle ne correspond pas aux bords sur toute sa surface, reprendre la photo bien de face.

**Pièce :** placer A et B sur deux bords opposés, en passant par le centre. Le cercle affiché doit coïncider avec le bord extérieur. Une pièce apparaissant elliptique signale une prise de vue oblique : ce mode ne corrige pas cette déformation.

**Mesure personnalisée :** placer A/B sur une longueur connue, saisir sa distance réelle en millimètres puis **Appliquer**. Les références prédéfinies remplissent et verrouillent cette valeur automatiquement.

Changer le type de référence efface A/B afin d'éviter de réutiliser une ancienne calibration. Le contour et les traits restent en place. **Annuler** permet de revenir à la référence précédente.

## Traits de mesure, puis contour

1. Dans **Traits → Axe O–X**, placer **O**, origine des mesures, puis **X**, qui donne la direction de l'axe. Ces points peuvent être déplacés.
2. Passer sur **Placer les traits**, puis toucher la photo pour créer un point. Le trait passe perpendiculairement à l'axe et un segment relie le point à cet axe. Jusqu'à 64 traits peuvent être ajoutés.
3. Sélectionner et déplacer un point. La loupe apparaît pendant le déplacement. Les coordonnées **X/Y en mm** indiquent la distance le long de l'axe et l'écart signé à celui-ci. Une valeur négative indique l'autre côté de l'origine ou de l'axe.
4. Pour un réglage numérique, saisir X et Y puis **Déplacer**. **Retirer le trait sélectionné** supprime seulement ce repère.
5. Construire le contour dans **Contour** en plaçant ses points dans l'ordre. Depuis **Traits**, **Copier ce point dans le contour** ajoute l'intersection sélectionnée à un contour ouvert. Cette copie est indépendante : déplacer ensuite le trait ne déplace pas le point du contour.
6. Fermer le contour, puis corriger ses points dans **Ajuster**. Mesurer l'épaisseur et **Appliquer** avant la 3D et l'export.

Les traits sont des repères de mesure manuels. Ils ne constituent pas un décodage automatique, une base de données de fabricants ni une reconstruction des surfaces invisibles. Les exports SVG/STL utilisent le contour fermé, pas les traits seuls.

Le déplacement de la vue se fait à deux doigts et le zoom par pincement. Le bouton d'agrandissement en haut à droite ouvre le mode actif en **plein écran**. **Cadrer** recentre la photo ; **Terminé** ramène aux champs et aux autres modes. Annuler/Refaire conserve aussi les changements de référence, l'axe et les traits.

La fonction **Échelle écran** permet un étalonnage avec une règle, puis l'affichage du contour selon cette échelle. Un pincement ultérieur quitte cette échelle et revient au zoom libre. Vérifier de nouveau l'étalonnage après un changement d'appareil ou de zoom d'affichage.

## Exports et limites

- **JSON** : relevé modifiable, y compris photo, référence choisie, A/B, axe, traits et contour. Le format version 2 lit les anciens relevés version 1. Les anciennes applications 0.1.x ne lisent pas les nouveaux relevés version 2.
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

La publication nécessite aussi la réussite d'un essai natif sur un simulateur **iPhone SE de troisième génération, 375 × 667 points**, avec un environnement iOS installé sur le runner. Il vérifie les références, la migration JSON, les coordonnées d'un axe tourné, le déplacement numérique, Annuler/Refaire et le rendu des vues carte/pièce/traits. Les captures et rapports se trouvent dans l'artefact de vérification du workflow. Ce simulateur n'est pas l'iPhone SE 2020 physique sous iOS 17.5 : caméra, précision de la photo, jailbreak et installation sur ce téléphone nécessitent encore un essai réel.

Les sources de vérification ne sont compilées que dans l'application de simulateur et ne sont pas ajoutées au paquet distribué.

Avant la compilation, un test Linux effectue un véritable dépaquetage dpkg dans des racines temporaires isolées, dont une avec /var/jb représenté par un lien symbolique. Les fichiers de test ne sont jamais exécutés et la base de paquets de l'hôte n'est pas modifiée. Ce test vérifie la structure et les permissions nécessaires à l'installation ; il ne remplace pas la configuration ni l'exécution sur iOS.

Les tests de géométrie et d'emballage peuvent aussi être exécutés sur Linux :

    python3 scripts/build_keylab.py --test-only

L'artefact de compilation contient le .deb et le rapport validation.json. Les tests d'hôte et la compilation ne remplacent pas un essai de UIKit, de la caméra, du partage et des mesures physiques sur l'iPhone cible.
