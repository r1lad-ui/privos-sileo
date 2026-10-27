# PRIV OS LAB 0.5.1

Mise à jour expérimentale pour iPhone SE 2020, iOS 17.5, Dopamine rootless.

## Correction Centre de contrôle

Le paquet 0.5.0 utilisait `CFBundlePrincipalClass` dans les Info.plist des deux modules, alors que Foundation reconnaît `NSPrincipalClass`. La version 0.5.1 corrige ce champ et ajoute les métadonnées de taille et de plateforme. Elle déclare également CCSupport comme dépendance.

**L'application PRIV Control et son coffre ne changent pas.** Les modules restent expérimentaux : ils ne sont pas validés sur le matériel de l'utilisateur.

## Mise à jour

Installer depuis Sileo → Sources → PRIV OS → Mise à jour, puis effectuer un unique respring depuis Dopamine si l'option existe. Ouvrir Réglages → Centre de contrôle ; rechercher `PRIV Control` et `Effacer texte copié`.

Si les modules ne s'affichent pas, ne réinstallez pas le jailbreak ; relever la liste des bundles de `/var/jb/Library/ControlCenter/Bundles/` et les journaux de SpringBoard. Si un Safe Mode apparaît, redémarrer l'iPhone sans injection des tweaks puis désinstaller le module défectueux.

## Limites

Pas de VPN, de coupure matérielle caméra/micro, ni de système iOS alternatif. Le dépôt APT n'a pas de Release signé. Ne pas utiliser comme produit de sécurité audité.
