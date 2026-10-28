# Sécurité — PRIV Control 0.5.5

L’écran **Sécurité** aide à vérifier les premières conditions de verrouillage et à retrouver les réglages à appliquer sur l’iPhone. Les protections du téléphone se configurent dans **Réglages** ; PRIV ne les modifie pas automatiquement.

## Ouvrir le nouvel écran

Dans Sileo, actualiser le dépôt PRIV OS puis installer **PRIV OS LAB 0.5.5 (Sécurité et fonds)**. Fermer et rouvrir PRIV Control. L’accueil affiche **v0.5.5**, avec les boutons **Sécurité** et **Fonds d’écran**.

## Ce que le diagnostic signifie

| Indication | Ce qu’elle permet de conclure |
|---|---|
| Code iOS configuré | iOS déclare disponible l’authentification du propriétaire, qui exige un code. La longueur et la qualité du code ne sont pas connues de PRIV. |
| Aucun code configuré | iOS renvoie explicitement l’erreur « code non défini ». |
| Code non vérifiable | L’API est indisponible ou retourne une autre erreur. PRIV ne transforme pas cette incertitude en réussite ou en absence de code. |
| Touch ID disponible pour PRIV | La biométrie est utilisable par l’app à cet instant. Cela ne prouve pas que l’option de déverrouillage de l’iPhone par Touch ID est activée. |
| Biométrie non configurée, bloquée ou indisponible | Le résultat dépend de l’erreur renvoyée par iOS. En cas de blocage temporaire, réactiver la biométrie avec le code système selon les indications d’iOS. |

Le diagnostic est refait à l’ouverture, avec **Actualiser** et au retour dans l’app. La version affichée est celle déclarée par iOS. Aucun code ni aucune empreinte ne sont lus ; le diagnostic ne demande pas de saisie du code dans PRIV et ne lance pas d’authentification.

Le délai de verrouillage, les accès depuis l’écran verrouillé, Localiser, la protection en cas de vol et les réglages des autres apps restent **à vérifier dans Réglages**. Aucun score ni témoin « tout est sécurisé » n’est affiché.

## Réglages conseillés sur l’iPhone SE

1. **Code et Touch ID.** Dans **Réglages → Touch ID et code**, utiliser un code long et unique, idéalement alphanumérique, et ne garder que les empreintes voulues. Pour changer le format : **Changer le code → Options de code**. Si quelqu’un connaît l’ancien code et que l’option est proposée, le faire expirer après changement.
2. **Verrouillage rapide.** Dans **Réglages → Luminosité et affichage → Verrouillage automatique**, choisir **30 secondes**, si cette durée est proposée.
3. **Accès avant déverrouillage.** Dans **Touch ID et code → Autoriser l’accès en mode verrouillé**, désactiver les accès inutiles, notamment Centre de contrôle, Siri et les réponses aux messages. Sous iOS 17, laisser **Accessoires USB** désactivé pour conserver la restriction avant déverrouillage.
4. **Notifications.** Dans **Réglages → Notifications → Afficher les aperçus**, choisir **Si déverrouillé** afin de masquer le contenu des messages tant que le téléphone est verrouillé.
5. **Protection en cas de vol.** Si la fonction est proposée, dans **Touch ID et code → Protection en cas de vol de l’appareil**, l’activer et choisir **Toujours** lorsque cette option existe. Elle nécessite notamment Touch ID, un code, l’identification à deux facteurs du compte Apple, Localiser et les Lieux importants.
6. **Prêter une seule app.** Utiliser **Réglages → Accessibilité → Accès guidé** pour limiter temporairement l’iPhone à l’application prêtée. Configurer son code et vérifier comment terminer la session avant de donner le téléphone.

Le bouton **Ouvrir Réglages** ouvre l’application Réglages. Il ne garantit pas l’ouverture directe du sous-menu Touch ID et code.

## Confidentialité de l’aperçu de PRIV

Quand PRIV devient inactive ou passe en arrière-plan, un écran opaque **Contenu masqué** couvre sa fenêtre. Il vise à remplacer l’aperçu privé dans le sélecteur d’applications. Au retour, le cache se retire et le coffre garde son fonctionnement actuel : son mot de passe reste nécessaire après verrouillage.

Ce cache ne bloque pas les captures d’écran réalisées pendant l’utilisation et n’agit pas sur les autres applications. Le diagnostic ne certifie pas l’intégrité d’un système jailbreaké.

## Vérification sur le téléphone

La compilation, les scénarios simulés et le paquet sont contrôlés avant publication. L’API réelle d’iOS et son aperçu doivent encore être vérifiés sur l’iPhone : ouvrir **Sécurité**, revenir depuis Réglages, puis regarder la vignette PRIV dans le sélecteur d’applications. Pour cet essai, utiliser uniquement du texte de démonstration dans le coffre.

## Sources Apple

- [Définir un code — guide iOS 17](https://support.apple.com/fr-fr/guide/iphone/iph14a867ae/17.0/ios/17.0)
- [Accès depuis l’écran verrouillé — guide iOS 17](https://support.apple.com/fr-fr/guide/iphone/iph9a2a69136/17.0/ios/17.0)
- [Aperçus des notifications](https://support.apple.com/fr-fr/guide/iphone/iph7c3d96bab/ios)
- [Ancien code après changement](https://support.apple.com/fr-fr/guide/personal-safety/ipsd0a253dd5/web)
- [Protection en cas de vol de l’appareil](https://support.apple.com/fr-fr/120340)
- [Accès guidé](https://support.apple.com/fr-fr/111795)
- [Disponibilité d’une politique d’authentification](https://developer.apple.com/documentation/localauthentication/lacontext/canevaluatepolicy(_:error:))
- [Type de biométrie](https://developer.apple.com/documentation/localauthentication/lacontext/biometrytype)
- [Protection des aperçus d’applications](https://developer.apple.com/library/archive/qa/qa1838/_index.html)
