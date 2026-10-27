# PRIV OS LAB 0.5.2 — Correction de chargement dans le Centre de contrôle

La 0.5.1 permettait l'affichage des modules dans Réglages → Centre de contrôle mais pas sur le panneau de commandes.

## Changements techniques

1. « PRIV Control » utilise maintenant `CCUIAppLauncherModule` (classe système iOS) avec `CCAssociatedBundleIdentifier=fr.privos.control`, comme dans l'exemple officiel `CydiaLauncherModule` de CCSupport. Un petit binaire d'extension signé accompagne les métadonnées ; aucun hook SpringBoard.
2. « Effacer texte copié » est recompilé comme une vraie sous-classe Objective-C de `CCUIToggleModule`, liée à `ControlCenterUIKit`, avec `NSPrincipalClass` permettant sa découverte par Foundation. L'ancienne classe créée dynamiquement dans un constructeur a été supprimée.
3. Les deux bundles possèdent des icônes de réglages et un format de taille 1×1. Les clés des `Info.plist` ont été vérifiées.
4. L'application PRIV Control et son exécutable ne sont **pas modifiés**. Le stockage du coffre chiffré est conservé à son emplacement antérieur.
5. Aucun script ne modifie SpringBoard, ne redémarre automatiquement l'interface, ou ne change les réglages caméra, micro et VPN.

## Installation prudente

Depuis Sileo → Sources → PRIV OS → installer la mise à jour 0.5.2. Vérifier qu'aucun composant essentiel n'est proposé à la suppression. Effectuer un **respring une seule fois** si requis ; ouvrir le Centre de contrôle.
Tester d'abord « PRIV Control », puis « Effacer texte copié » avec un texte de test non sensible.

En cas de nouveau Safe Mode, redémarrer, désactiver `Tweak Injection` dans Dopamine avant le jailbreak, puis retirer la version depuis Sileo sans retirer ElleKit et autres composants système.

Il n'existe aucune garantie que les modules fonctionneront sur l'appareil. Les interfaces internes Apple sont privées, et PRIV OS LAB ne garantit **aucune sécurité supplémentaire** en lui-même. Le dépôt APT n'est pas signé par une clé de confiance. Ne pas mettre de secrets sensibles dans un prototype.
