# Activer les icônes PRIV OS — iPhone SE 2020 / iOS 17.5

Les **20 icônes PRIV OS** sont incluses dans PRIV OS LAB 0.5.2, sous `/var/jb/Library/Themes/PRIV OS 0.5.theme/`. Aucun nouveau téléchargement d'icônes n'est nécessaire.

## Activation

1. Dans **Sileo**, cherche **LumenBoard**, éditeur **xsxs18**. Si absent, ajoute le dépôt **https://havoc.app/**.
2. Installe la version **rootless** (`iphoneos-arm64`) compatible iOS 17.5 / Dopamine.
3. Vérifie que **SnowBoard, Anemone et NeonBoard ne sont pas installés simultanément**. Ne réinstalle pas SnowBoard sur cet appareil.
4. Ouvre **LumenBoard**.
5. Active le thème **PRIV OS 0.5** dans sa liste de thèmes puis appuie sur **Apply**.
6. Laisse le moteur appliquer les icônes et faire le respring proposé. Vérifie ensuite Téléphone, Messages, Safari, Photos, Réglages.

Si le thème est absent, vérifier l'emplacement `/var/jb/Library/Themes/PRIV OS 0.5.theme/` dans Sileo → paquet PRIV OS → Afficher les fichiers installés. Les icônes peuvent également apparaître dans LumenBoard sous un nom légèrement différent.

Si un Safe Mode apparaît, ne multiplie pas les resprings ; redémarre normalement, réactive Dopamine avec l'injection des tweaks désactivée et retire LumenBoard avant de reprendre le diagnostic.

## Transparence

LumenBoard n'est **pas inclus** dans le paquet PRIV OS LAB actuel : c'est un moteur tiers que l'utilisateur installe une fois. Le thème comporte 20 icônes principales, donc d'autres applications conserveront leurs icônes d'origine. Le logo Apple au démarrage n'est pas modifié.

Source officielle : https://github.com/xsxs18-dev/LumenBoard
Paquet Havoc : https://havoc.app/package/lumenboard
