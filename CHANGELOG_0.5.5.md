# PRIV OS LAB 0.5.5 — Sécurité et fonds

Cette version poursuit la demande de sécurisation du téléphone avec un écran natif de diagnostic, les chemins vers les réglages utiles et un cache de confidentialité pour l’aperçu de PRIV.

## Changements

- Bouton **Sécurité** à gauche et **Fonds d’écran** à droite sur l’accueil.
- Diagnostic ponctuel de la disponibilité du code iOS et de la biométrie, avec `LAContext.canEvaluatePolicy:error:` ; aucun dialogue d’authentification n’est lancé.
- Affichage de la version déclarée par iOS. Les réponses non interprétables restent indiquées comme inconnues.
- Nouveau contexte à chaque actualisation et vérification au retour dans l’app : les états ne sont pas enregistrés comme des garanties durables.
- Guide de verrouillage et bouton pour ouvrir l’application Réglages. PRIV ne change ni code, ni empreinte, ni préférence système.
- Cache opaque de la fenêtre lorsque l’app devient inactive ou passe en arrière-plan ; retrait au retour actif, sans déverrouillage du coffre.
- Couleurs partagées retenues explicitement en mémoire afin qu’elles restent valides après le démarrage, notamment pour les messages d’erreur affichés plus tard.
- Largeur des deux boutons d’accueil adaptée aux écrans logiques de 320 et 375 points, pour éviter un chevauchement en affichage agrandi.

## Conservation des fonctions précédentes

Le code du coffre, de son chiffrement, du fichier de données, de l’import photo et de sa fonction historique de verrouillage est identique aux sources de référence. Le nouveau délégué ajoute le cache puis appelle cette fonction existante. Aucun nouveau mot de passe n’est stocké et aucune migration du coffre n’est effectuée.

Le module de fonds d’écran 0.5.4 est inchangé, ainsi que les images, les icônes, les modules du Centre de contrôle et les scripts d’installation. Les callbacks Photos continuent leur traitement même pendant une interruption temporaire. Aucun respring n’est déclenché par le paquet.

## Interprétation du diagnostic

Un code est indiqué comme absent uniquement si iOS renvoie `LAErrorDomain` et `passcodeNotSet`. Une erreur différente ne devient pas une absence de code. La biométrie est nommée selon `biometryType`, consulté après le test de disponibilité, et les erreurs d’inscription, de blocage et d’indisponibilité sont distinguées.

**Touch ID disponible pour PRIV** n’est pas une preuve que Touch ID déverrouille le téléphone. Le diagnostic ne connaît pas la robustesse du code, le délai de verrouillage, les accès de l’écran verrouillé ou l’état de la protection en cas de vol. Ces réglages restent manuels. Le cache concerne l’aperçu de PRIV ; il ne bloque pas toutes les captures d’écran ni l’accès aux autres apps.

## Validation

La publication exige les tests du diagnostic et du cache, les 19 scénarios existants d’export Photos, la compilation arm64, la vérification de chaque page de signature ad hoc et la comparaison du paquet avec 0.5.4. Seuls l’exécutable, les versions du `Info.plist` et les métadonnées du paquet peuvent changer ; le guide `SECURITE.txt` est ajouté.

Résultat : **21 scénarios Sécurité et 19 scénarios Photos réussis**. Deux compilations indépendantes de la source finale produisent le même exécutable, sans avertissement ; ses 25 pages de signature sont vérifiées. La comparaison complète conserve 121 fichiers du thème et 12 fichiers des modules, ainsi que les scripts d'installation et les autres ressources.

- Paquet : `debs/PRIV_OS_0_5_5_Sileo_iphoneos-arm64.deb`, **2 284 056 octets**.
- SHA-256 du paquet : `7f893a978ebfb09340b6a1546e2e719296a3d6d783a86d42a9adbac52c312866`.
- SHA-256 de l'exécutable : `417b325a1a824e60b234e77652f04114631fbc3e9294422e0b89636af9830ae6`.

**Pas d’exécution sur l’iPhone depuis cet environnement.** Les résultats LocalAuthentication, la vignette réelle du sélecteur d’applications et les interactions avec les autorisations Photos restent à confirmer sur l’appareil.

[Utilisation et réglages](SECURITE.md) · [Construction et tests](native/README.md)
