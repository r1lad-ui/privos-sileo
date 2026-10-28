# PRIV OTP — prototype de code temporaire

**État : moteur de calcul et démonstration. Aucun verrouillage de l’iPhone n’est ajouté par ce dossier.**

L’objectif est d’étudier un code qui change toutes les minutes à partir d’un secret aléatoire. Le calcul suit [TOTP, RFC 6238](https://www.rfc-editor.org/rfc/rfc6238), avec HMAC-SHA256. Connaître l’heure ne suffit donc pas à calculer le code sans posséder le secret.

Ce prototype est développé sur une branche séparée du dépôt PRIV OS. Il ne contient aucun hook SpringBoard, aucun paquet à installer et aucune modification du code iOS. Il ne change pas la version Sileo 0.5.5.

## Ce qui est développé

| Élément | Fonction |
| --- | --- |
| [Moteur C](core/priv_totp.h) | Génération, validation stricte et état contre la réutilisation d’une période déjà acceptée. |
| [Tests C](core/test_totp.c) | Vérification avec OpenSSL et les valeurs publiques de référence de la RFC, plus les limites de temps et de saisie. |
| [Démo hors ligne](demo/index.html) | Affichage du code et du temps restant dans un navigateur avec Web Crypto. Clé de test publique uniquement. |
| [Tests de la démo](demo/) | Comparaison du calcul du navigateur avec les valeurs de référence. |

La démonstration sert à voir le fonctionnement sur un ordinateur, notamment un Chromebook. Télécharger son fichier `index.html`, puis l’ouvrir dans un navigateur compatible avec Web Crypto. Si le navigateur ne rend pas cette fonction disponible pour un fichier local, la page l’indique et refuse le calcul. Aucune commande de terminal n’est nécessaire pour ouvrir le fichier.

**Les codes de cette démo sont publics. Ils ne protègent aucun appareil.** Le prototype ne crée, ne demande, ne transfère et ne conserve aucun vrai secret d’utilisateur.

## Profil retenu pour l’étude

| Paramètre | Valeur |
| --- | --- |
| Algorithme | HMAC-SHA256 fourni par une bibliothèque cryptographique |
| Secret d’une future installation | 32 octets aléatoires, uniques, distincts du code iOS |
| Code affiché | 6 chiffres, zéros initiaux conservés |
| Période | 60 secondes, à partir de l’époque Unix |
| Périodes acceptées par défaut | La période courante uniquement |
| Réutilisation | Une seule acceptation par période pour un même secret et une même configuration |

La RFC recommande une période par défaut de 30 secondes. Le choix de 60 secondes correspond ici à la demande du projet. Un futur authentificateur externe devra effectivement respecter **SHA256, 6 chiffres et 60 secondes** ; sa simple prise en charge de « TOTP » ne suffit pas à le supposer.

Une option de l’API générique peut accepter la période précédente pour terminer une saisie au changement de minute. Cette option est désactivée dans le profil normal : l’activer peut prolonger la validité d’un code jusqu’à presque 120 secondes et augmente le nombre de codes acceptables. Aucune période future n’est acceptée.

Le refus de réutilisation porte sur le compteur de période, pas sur l’unicité éternelle des six chiffres : des valeurs peuvent se répéter. La comparaison parcourt tous les chiffres sans arrêt anticipé selon leur contenu. Cela ne constitue pas à elle seule une preuve que l’ensemble du programme s’exécute en temps constant sur toutes les plateformes.

## Ce que l’intégration iOS doit encore résoudre

### Le code Apple garde son rôle

Le code iOS participe à la protection des clés de chiffrement du téléphone ; l’authentification biométrique suit également le mécanisme Apple. Un résultat TOTP calculé par un tweak ne remplace pas ce mécanisme. Voir [Codes et mots de passe](https://support.apple.com/guide/security/sec20230a10d/web) et [Utilisation de Touch ID et des autres biométries](https://support.apple.com/guide/security/secc5227ff3c/web).

La piste étudiée serait une étape logicielle supplémentaire, puis une authentification Apple indépendante. Aucune version du projet ne doit lire, mémoriser, rejouer ou changer automatiquement le vrai code iOS pour faire croire qu’un TOTP le remplace. Un échec TOTP ne doit pas être envoyé comme une tentative de code Apple.

Un simple écran affiché après le déverrouillage ne prouve pas que les données sont restées verrouillées. Même retarder l’animation de l’écran verrouillé ne le prouve pas : les chemins réels d’authentification, notamment Touch ID, doivent être contrôlés et testés sur la version exacte du téléphone.

### Le lecteur du code doit être accessible

Le code doit être lisible pendant que l’iPhone est verrouillé, sur un autre appareil ou un dispositif autonome. Le montrer directement sur l’écran verrouillé de ce même iPhone permettrait aussi à une autre personne de le lire.

Un Chromebook est une possibilité, mais il faudrait l’avoir à portée de main pour les déverrouillages concernés. Le choix entre cet usage et une exigence « iPhone seul » doit être précisé avant de concevoir l’activation.

### Le stockage du secret dépend de l’étape choisie

La future création du secret doit utiliser le générateur aléatoire du système, sur l’appareil, après authentification native. L’association avec le lecteur externe devra être confirmée par un code valide avant activation. Une clé, un QR code ou une URI d’association révèle le secret complet : aucun de ces éléments ne doit passer par les journaux, un dépôt public, une URL distante ou le presse-papiers par défaut.

La classe Keychain [`WhenPasscodeSetThisDeviceOnly`](https://developer.apple.com/documentation/security/ksecattraccessiblewhenpasscodesetthisdeviceonly) convient à des contrôles accessibles après déverrouillage, mais pas à une lecture du secret pendant que l’appareil reste verrouillé. [`AfterFirstUnlockThisDeviceOnly`](https://developer.apple.com/documentation/security/ksecattraccessibleafterfirstunlockthisdeviceonly) permet un accès après le premier déverrouillage, avec une exposition plus longue à documenter et à évaluer. Aucun de ces choix ne doit être arrêté sans décider où intervient le contrôle.

Une validation HMAC logicielle lit son secret en mémoire. Il serait donc incorrect de promettre que ce secret ne quitte jamais le Secure Enclave ou qu’il résiste à un adversaire contrôlant le jailbreak.

### Les essais et la reprise nécessitent une intégration dédiée

L’état de non-réutilisation fourni par le moteur vit en mémoire. L’appelant devra le lier au secret et à sa configuration, sérialiser les accès concurrents et **enregistrer durablement le nouvel état avant toute autorisation**. Une erreur de stockage doit refuser l’autorisation ; relancer un processus ne doit pas remettre silencieusement l’état à zéro.

Le moteur ne contient pas de limitation du nombre d’essais. Une future intégration devra ajouter une temporisation sur une horloge monotone incluant la veille, un état de reprise conservateur et une politique explicite face aux changements d’heure. Six chiffres sans limitation d’essais ne constituent pas une protection suffisante.

L’autorisation TOTP devra être courte, consommable une seule fois et liée au cycle de verrouillage concerné. Elle devra disparaître au reverrouillage, à l’expiration et après un crash ou un relancement.

### Limite du jailbreak et récupération

[Dopamine se présente comme un jailbreak semi-untethered](https://github.com/opa334/Dopamine). Une protection fournie par un tweak ne doit donc pas être présentée comme une règle matérielle obligatoire, permanente et indépendante du jailbreak. Si le tweak ne fonctionne pas, le verrouillage Apple doit rester opérationnel. Une personne connaissant le code iOS et pouvant démarrer sans le tweak pourrait alors contourner cette étape supplémentaire.

Avant toute intégration, il reste à confirmer le modèle, la version **et le build iOS**, ainsi que les versions du jailbreak et du moteur d’injection. Les essais réels devront couvrir Touch ID, le code natif, le redémarrage, le mode sans tweak, les notifications, les appels entrants, SOS, les appels d’urgence, la fiche médicale, VoiceOver et la récupération. Aucun hook ne doit s’activer sur une version non prise en charge.

## Validation et portée des résultats

Les tests d’algorithme utilisent exclusivement la clé SHA256 publique de la RFC : la chaîne ASCII `12345678901234567890123456789012`. **Cette chaîne n’est jamais un secret de production.** HMAC est réalisé par OpenSSL dans les tests C et par Web Crypto dans la démo ; le projet ne réimplémente ni SHA256 ni HMAC.

Les tests sur ordinateur prouvent le comportement des fonctions testées. Une compilation en objet ARM64 vérifie la construction du moteur pour cette architecture ; elle ne prouve ni une exécution sur iOS, ni la disponibilité d’un fournisseur HMAC sur le téléphone, ni la sûreté d’un hook.

Les commandes de validation destinées au développement et leurs résultats sont détaillés dans [VALIDATION.md](VALIDATION.md). Aucun test de verrouillage n’a été exécuté sur l’iPhone cible.
