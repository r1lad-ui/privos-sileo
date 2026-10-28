# PRIV OS LAB 0.5.3 — icônes et fonds

28 octobre 2026.

## Résultat

- 32 applications supplémentaires et une icône Calendrier corrigée, soit **53 identifiants d'applications couverts** avec les ressources précédentes.
- 33 sources SVG éditables, leurs PNG installables en 512 × 512, et un manifeste d'association aux applications vérifié.
- Fond **PRIV Nuit** précédemment créé pour ce thème, accessible au téléchargement et ajouté aux ressources du paquet ; fond classique conservé.
- Le Calendrier n'affiche plus un « 27 » permanent. Le nouveau pictogramme est sans date.
- Le thème conserve son nom et son emplacement **PRIV OS 0.5**, pour la configuration LumenBoard existante.

## Périmètre technique

Cette version met à jour les ressources du paquet `fr.privos.control`. Elle reprend la base 0.5.2 dont le SHA-256 est `2f6168d842cff85e579cd0a83b4094ce1bb1aedc31b1d38916fb67406d740aae`.

Les fichiers de l'application native et des deux modules du Centre de contrôle, ainsi que les scripts `postinst` et `postrm`, sont conservés **octet pour octet**. Le numéro de version interne de l'application reste donc 0.5.2 ; le paquet distribué par Sileo porte le numéro 0.5.3. Aucune migration des données du coffre n'est exécutée. Les scripts existants ne forcent aucun respring.

Les images ne changent aucune permission système, aucun code de verrouillage, aucun réglage Touch ID, ni aucun réglage réseau. Les propositions de sécurité nécessitent une configuration distincte dans les réglages iOS et dans les applications.

## Reconstruction

Prérequis : Python 3, Pillow, CairoSVG (ou Inkscape), `dpkg-deb`.

```bash
python3 scripts/render_icons.py --preview build/icons-preview.png
python3 scripts/build_theme_release.py
```

Le script de construction refuse une base dont l'empreinte a changé et refuse toute modification inattendue des fichiers existants. Il produit le `.deb`, `Packages`, `Packages.gz`, `Release` et un rapport local `build/validation-0.5.3.json`. Les archives historiques restent dans `debs/`.

## Vérification sur iPhone

Après mise à jour dans Sileo, ouvrir LumenBoard et utiliser **Apply**. Vérifier les applications mentionnées dans [ICONES.md](ICONES.md). Si nécessaire, utiliser une seule fois l'action officielle de LumenBoard **Clear icon cache & respring**.

La structure et les ressources sont vérifiées ici, mais aucun test d'exécution de cette version sur l'iPhone n'a été effectué. En particulier, une date dynamique pour Calendrier et l'effet des icônes alternatives d'apps ne sont pas garantis. Les limites du prototype natif et l'absence de signature du dépôt APT restent celles de la 0.5.2.
