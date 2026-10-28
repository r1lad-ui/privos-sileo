# Aperçu PRIV OTP

Ouvrir `index.html` dans Chrome permet de voir un code à six chiffres changer chaque minute. Le fichier est autonome et fonctionne hors ligne si le navigateur autorise Web Crypto pour les fichiers locaux. Il ne demande aucune installation, aucun compte et aucun secret personnel.

**C'est une démonstration utilisant une clé publique, sans protection réelle et sans connexion au verrouillage de l'iPhone.** Elle n'est pas un authentificateur à configurer pour un usage réel.

La clé publique est le vecteur SHA-256 du [RFC 6238, annexe B](https://www.rfc-editor.org/rfc/rfc6238#appendix-B) : `12345678901234567890123456789012` en UTF-8, soit 32 octets. Le profil affiché utilise HMAC-SHA-256, six chiffres, une période de 60 secondes et l'époque Unix comme origine. La page est un générateur ; elle ne valide aucun accès et n'applique donc pas l'état anti-rejeu du moteur C.

## Vérification pour le développement

Depuis la racine du dépôt, avec une version récente de Node disposant de Web Crypto :

```sh
node experimental/priv-otp/demo/test_demo.mjs
```

Le test importe uniquement le module embarqué dans ce fichier du dépôt. Il vérifie les six vecteurs SHA-256 officiels (30 secondes, huit chiffres), les fixtures du profil PRIV avec un HMAC indépendant Node/OpenSSL, les limites de minute, les zéros initiaux, les erreurs de saisie dans l'API interne, l'affichage en cas d'absence de Web Crypto et une réponse asynchrone arrivée après le changement de minute.

Une politique CSP interdit les ressources et connexions externes ; le script embarqué est autorisé par son empreinte SHA-256. Toute modification du script nécessite de recalculer cette empreinte dans la balise CSP, faute de quoi le navigateur le bloquera et le test échouera. Aucun import de bibliothèque, stockage persistant ou appel réseau n'est utilisé.
