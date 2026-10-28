# Validation du prototype PRIV OTP

Validation du 28 octobre 2026, sur ordinateur Linux. Le périmètre est le moteur TOTP et la démonstration avec une clé publique. **Aucun test sur l’iPhone et aucun déverrouillage système n’ont été effectués.**

## Résultats exécutés

| Vérification | Résultat | Portée |
| --- | --- | --- |
| C11, OpenSSL, compilation optimisée avec avertissements traités en erreurs | 7 groupes, 370 assertions réussies | Génération et vérification du moteur C |
| AddressSanitizer et UndefinedBehaviorSanitizer | Même suite réussie | Erreurs mémoire et comportements indéfinis détectables sur ces scénarios |
| Web Crypto sous Node | 41 contrôles réussis, plus vérifications CSP et absence de ressources externes | Calcul JS, frontières temporelles, états d’interface simulés et réponse asynchrone tardive |
| Compilation croisée Clang 18 pour `arm64-apple-ios15.0` | Objet Mach-O ARM64 de 2 920 octets produit | Construction du moteur seul ; aucun lien avec iOS, aucun fournisseur HMAC Apple exécuté |
| Relecture indépendante et calcul Python des fixtures | Aucun défaut bloquant trouvé | Relecture du moteur, de la démo et des limites annoncées |
| Contrôle des fichiers existants | Aucun fichier préexistant modifié | App native, fonds, thème, coffre, paquets et index Sileo préservés |

LeakSanitizer ne fonctionne pas sous le tracing de cet environnement ; il a été désactivé explicitement pour le passage ASan/UBSan. Ce résultat ne vaut donc pas vérification des fuites mémoire. Le rendu réel de la démo dans Chrome n’a pas été contrôlé : aucun exécutable de navigateur n’était disponible. Les tests de ses états d’interface utilisent un document simulé.

## Scénarios du moteur

Les sept groupes couvrent :

1. Les six vecteurs officiels SHA256 de la RFC 6238, à 30 secondes et huit chiffres.
2. Le profil PRIV à 60 secondes et six chiffres : frontières de minute, zéro initial, compteur supérieur à 32 bits et `INT64_MAX`.
3. Les saisies strictes : longueur, chiffres ASCII, caractères non valides, absence de normalisation implicite.
4. Les fenêtres de validation, le refus du rejeu, le recul d’horloge, T0 et le refus de la période future.
5. Les erreurs du fournisseur HMAC, y compris après le calcul d’un premier candidat correct, et l’évaluation des candidats configurés.
6. Une collision réelle entre deux périodes adjacentes de la clé publique, avec refus d’un candidat correspondant à une période déjà consommée.
7. Les paramètres et dates invalides, tailles de buffers, pointeurs nuls et conservation exacte des sorties et de l’état en cas d’échec.

La clé utilisée est exclusivement la clé **publique** de test SHA256. Les attendus du profil 60 secondes ont été calculés indépendamment avec `hmac` et `hashlib` de Python, puis vérifiés également par la démonstration avec Web Crypto et Node/OpenSSL.

## Reproduire les tests de développement

Depuis la racine du dépôt, avec un compilateur C11, OpenSSL et ses en-têtes de développement :

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror \
  experimental/priv-otp/core/priv_totp.c \
  experimental/priv-otp/core/test_totp.c \
  -lcrypto -o /tmp/priv-test-totp
/tmp/priv-test-totp
```

Pour la démonstration, utiliser une version récente de Node qui fournit Web Crypto :

```sh
node experimental/priv-otp/demo/test_demo.mjs
```

Pour les sanitizers, ajouter `-fsanitize=address,undefined -fno-omit-frame-pointer`, utiliser `-O1 -g` et exécuter la même suite. Dans cet environnement sous tracing seulement, l’exécution exigeait `ASAN_OPTIONS=detect_leaks=0`.

La compilation croisée du moteur seul utilisait Clang 18 avec :

```sh
clang -target arm64-apple-ios15.0 -ffreestanding -std=c11 -O2 \
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror \
  -c experimental/priv-otp/core/priv_totp.c -o /tmp/priv_totp.arm64.o
```

## Ce qui reste non développé ou non validé

Le fournisseur cryptographique iOS, l’enrôlement, le stockage Keychain, la persistance atomique contre le rejeu, la limitation des essais, l’interface native et les hooks de verrouillage ne font pas partie de ce prototype. Aucun secret personnel n’a été généré. Aucune garantie de sécurité du verrouillage ne peut être déduite de ces tests.

Le choix du lecteur externe et la confirmation de la version exacte de l’appareil conditionnent la suite décrite dans le [README](README.md).
