# PLAN — Jeu d'arène 1v1 (Unreal Engine 5)

Feuille de route à suivre dans l'ordre. Le contexte du projet (assets existants, tags, conventions, règles) est dans `CLAUDE.md`.
Chaque phase se termine par un critère « Terminé quand », à vérifier en PIE (Play In Editor) avant de passer à la suite.
Cocher `[x]` au fur et à mesure et tenir le Journal à jour en bas du fichier.

État de départ : l'arène est importée dans Unreal et le niveau est enregistré. Aucun code de gameplay n'existe encore.

---

## Phase 0 — Mise en route (1 session)

- [x] **0.1 Repérer le projet** : chemin du `.uproject`, version exacte d'UE 5, nom et chemin du niveau de l'arène. Les noter dans le Journal.
- [x] **0.2 Projet C++** : vérifier s'il existe un module C++ (`Source/`). Sinon, en ajouter un (Tools > New C++ Class… > `GameModeBase` suffit pour générer le module), puis compiler. Noter dans le Journal le nom du module, appelé `<Module>` dans la suite.
- [x] **0.3 Connecter Unreal MCP** :
  - Activer les plugins **Model Context Protocol** et **All Toolsets** (Edit > Plugins), puis redémarrer l'éditeur.
  - Ajouter `bAutoStartServer=True` sous `[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]`, dans `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`.
  - Dans la console de l'éditeur : `ModelContextProtocol.GenerateClientConfig ClaudeCode`, ce qui génère le `.mcp.json`.
  - Redémarrer Claude Code dans le dossier du projet, puis vérifier avec `list_toolsets`.
- [x] **0.4 Inventaire du niveau via MCP** : lister les acteurs tagués `slot`, `zone`, `chemin:*` et `batiment:*` avec leurs positions. Écrire dans le Journal l'axe « camp J → camp A » réel (+X, -X, +Y ou -Y) et les positions des 6 `SLOT_Tour_*` / `SLOT_Fort_*`.
- [x] **0.5 Contrôle de version** : `git init` avec un `.gitignore` Unreal (Binaries, Intermediate, Saved, DerivedDataCache, .vs). Activer Git LFS pour `*.uasset` et `*.umap`. Premier commit.
- [ ] **0.6 Caméra PC** : adapter `Camera_Arene` au 16:9 (FOV et hauteur) pour que tout le terrain soit visible, le camp J en bas de l'écran. Le PlayerController l'utilise comme caméra de vue (`SetViewTargetWithBlend`). Côté joueur A, la vue doit être retournée de 180° pour que chacun voie son camp en bas.

**Terminé quand** : MCP répond, le projet compile, et en PIE la vue montre toute l'arène depuis `Camera_Arene`.

---

## Phase 1 — Socle du gameplay en C++ (prototype solo)

Objectif : une partie jouable en solo contre une IA basique, avec des cubes et capsules à la place des modèles.

### 1.1 Architecture (classes C++ à créer dans `<Module>`)
| Classe | Rôle |
|---|---|
| `AArenaGameMode` | Démarre et termine la partie, fait apparaître les bâtiments et les unités. Serveur uniquement. |
| `AArenaGameState` | Répliqué : phase de jeu (Attente, EnCours, Prolongations, Fin), temps restant, couronnes par équipe, vainqueur. |
| `AArenaPlayerState` | Répliqué : équipe (`J`/`A`), élixir (float), deck (8 cartes), main (4 cartes) et carte suivante. |
| `AArenaPlayerController` | Entrées (sélection et glisser-déposer d'une carte), traçage du sol, prévisualisation, `ServerPlayCard(CardId, Location)`. |
| `UArenaCardData` (`UPrimaryDataAsset`) | Définition d'une carte : nom, icône, coût, type (Unite ou Sort), classe d'unité, nombre d'unités, statistiques. |
| `AArenaUnit` (`ACharacter`) | Unité : équipe, déplacement par AIController et NavMesh, ciblage, attaque, mort. |
| `AArenaUnitAIController` | Logique simple sans Behavior Tree au début : chercher une cible, aller vers elle, attaquer. Sinon suivre le chemin de sa voie. |
| `UArenaHealthComponent` | PV et PV max répliqués, `ApplyDamage`, événements `OnDamaged` et `OnDied`. Utilisé par les unités ET les bâtiments. |
| `UArenaAttackComponent` | Portée, cadence, dégâts, type de cible (Sol, Air, Bâtiments uniquement), projectile éventuel, dégâts de zone. |
| `AArenaProjectile` | Projectile simple (flèche, boule), dégâts à l'impact. |
| `AArenaBuilding` | Tour ou fort : composants Vie et Attaque, pilote les meshes importés (voir 1.4). |
| `AArenaAIOpponent` (`AInfo`) | Joueur IA pour le solo : joue une carte au hasard quand son élixir le permet. |

Règle : tout état de jeu est modifié sur le serveur. Les clients ne font qu'envoyer des intentions par RPC Server et affichent l'état répliqué.

### 1.2 Tâches
- [ ] **1.2.1** Créer les classes ci-dessus (squelettes compilables), puis des Blueprints enfants : `BP_ArenaGameMode`, `BP_ArenaPlayerController`, etc. Régler le GameMode du niveau dans World Settings.
- [ ] **1.2.2 Élixir** : maximum 10, départ à 5, +1 toutes les 2,8 s, ×2 pendant la dernière minute et les prolongations. Calculé sur le serveur, répliqué dans le PlayerState.
- [ ] **1.2.3 Deck et main** : un deck de 8 cartes mélangé, 4 en main et 1 « suivante ». Quand une carte est jouée, la suivante entre en main et la carte jouée retourne sous la pile.
- [ ] **1.2.4 Placement** : glisser une carte sur le terrain fait un trace sous la souris et affiche une prévisualisation. Le placement est valide si le point est dans la `ZONE_Deploiement` de son équipe et sur la NavMesh (les sorts peuvent viser partout). `ServerPlayCard` revérifie l'élixir et la zone, puis fait apparaître l'unité ou les unités avec un délai d'apparition de 1 s.
- [ ] **1.2.5 Unités** :
  - Ciblage : l'ennemi le plus proche dans un rayon de vision (6 m), en respectant le type de cible de l'unité. Une unité « Bâtiments uniquement » ignore les unités.
  - Sans cible, l'unité suit les `CHEMIN_IA_<équipe>_<voie>` de sa voie. La voie est choisie selon le côté du terrain où elle a été posée.
  - Elle s'arrête à portée, attaque à sa cadence, et change de cible si la cible meurt.
- [ ] **1.2.6 Bâtiments** (voir 1.4) : les tours tirent sur l'ennemi le plus proche à portée. Le fort est **inactif** tant qu'il n'a pas été touché et qu'aucune tour de son camp n'est détruite.
- [ ] **1.2.7 Règles de partie** :
  - Partie de 3:00, puis prolongations de 2:00 si les couronnes sont à égalité. La première couronne marquée en prolongations l'emporte.
  - Une tour détruite donne 1 couronne, le fort détruit donne 3 couronnes et la victoire immédiate.
  - Fin des prolongations à égalité : le camp dont le bâtiment le plus abîmé a le plus de PV en pourcentage gagne, sinon match nul.
- [ ] **1.2.8** `AArenaAIOpponent` joue le camp A en solo.
- [ ] **1.2.9 Débogage** : texte à l'écran avec élixir, chrono, couronnes et PV des bâtiments, en attendant la vraie interface (phase 3).

### 1.3 Cartes de départ (Data Assets `DA_Carte_*`, statistiques à équilibrer en phase 5)
Unités en formes simples pour l'instant : capsule colorée par équipe.

| Carte | Coût | Type | PV | Dégâts | Cadence | Portée | Vitesse | Cible | Notes |
|---|---|---|---|---|---|---|---|---|---|
| Écuyer | 3 | Unité (×1) | 650 | 90 | 1,2 s | 1 m | moyenne | Sol | mêlée de base |
| Arbalétrière | 3 | Unité (×2) | 220 | 60 | 1,0 s | 5 m | moyenne | Sol+Air | distance |
| Milice | 2 | Unité (×4) | 90 | 40 | 1,0 s | 1 m | rapide | Sol | essaim |
| Colosse de pierre | 6 | Unité (×1) | 2600 | 160 | 1,6 s | 1 m | lente | Bâtiments | tank |
| Bélier | 4 | Unité (×1) | 900 | 180 | 1,5 s | 1 m | rapide | Bâtiments | casseur de tours |
| Vouivre | 4 | Unité (×1) | 700 | 100 | 1,4 s | 2,5 m | moyenne | Sol+Air | volante, ignore la rivière |
| Boule de feu | 4 | Sort | – | 350 (zone 2,5 m) | – | – | – | Tout | 35 % des dégâts aux bâtiments |
| Mur de ronces | 3 | Sort | – | 0 | – | – | – | – | ralentit de 50 % pendant 4 s, zone de 3 m |

Bâtiments : tour 1400 PV, 50 dégâts toutes les 0,8 s, portée 7 m. Fort 2400 PV, 70 dégâts toutes les 1,0 s, portée 7 m.
Unités volantes : pas de NavMesh, déplacement direct (`FloatingPawnMovement`) à une hauteur fixe.

### 1.4 Intégration avec les bâtiments importés
- [ ] Au `BeginPlay`, le GameMode fait apparaître un `AArenaBuilding` sur chaque `SLOT_Tour_*` et `SLOT_Fort_*`. Il lit `type` et `equipe` dans les tags du slot, et récupère le groupe (`Tour_Gauche_J`…) depuis le nom du slot.
- [ ] Le bâtiment récupère ses 3 meshes avec les tags `batiment:<Groupe>` + `etat:<Etat>`, les flammes et lumières associées comprises.
- [ ] États pilotés par les PV :
  - au-dessus de 50 % : Intact ;
  - entre 0 et 50 % : Endommage ;
  - à 0 : Ruine.
  
  Au changement d'état : `SetActorHiddenInGame` et collision sur le bon mesh, et les autres sont cachés et sans collision. L'état est répliqué via `OnRep`.
- [ ] À la destruction : arrêt des tirs, couronne attribuée, événement `OnBuildingDestroyed` (pour les effets de la phase 4).
- [ ] Le `SLOT_Unite_<Groupe>` (sommet du bâtiment) sert de point de départ des projectiles de la tour. Plus tard, on y placera un tireur visible.

**Terminé quand** : en PIE, on pose des cartes contre l'IA, les unités traversent les ponts, se battent et attaquent les tours. Les tours passent par Intact, Endommage puis Ruine, les couronnes et le chrono fonctionnent, et la partie se termine avec un vainqueur.

---

## Phase 2 — Multijoueur à 2 (listen server)

- [ ] **2.1** Tester le code de la phase 1 avec 2 joueurs en PIE : Net Mode « Play As Listen Server », 2 joueurs. Corriger tout ce qui n'est pas répliqué.
- [ ] **2.2** Le premier connecté joue le camp J (hôte), le second le camp A. Caméra retournée pour A, chaque joueur ne peut déployer que dans sa zone.
- [ ] **2.3** Interpolation des déplacements des unités côté client (CharacterMovement répliqué), réplication des projectiles et des effets d'impact.
- [ ] **2.4** Connexion par IP pour commencer : menu « Héberger » (`open <Map>?listen`) et « Rejoindre » (`open <IP>`). Port 7777 à ouvrir sur la box de l'hôte.
- [ ] **2.5** Ensuite, connexion sans ouvrir de ports : Epic Online Services (sessions et relais P2P, gratuit), ou Steam si tout le monde l'utilise. Vérifier la documentation de la version d'UE installée avant de choisir.
- [ ] **2.6** Test avec latence simulée (`Net PktLag=100`, `Net PktLoss=2`) et entre 2 PC réels.

**Terminé quand** : deux PC jouent une partie complète, sans désynchronisation visible.

---

## Phase 3 — Interface (UMG)

- [ ] **3.1 HUD de partie** (`WBP_HUD`) : barre d'élixir segmentée, 4 cartes (icône, coût, grisée si l'élixir manque) et la suivante en aperçu, chrono, couronnes des deux camps, alerte « ×2 élixir » et « Prolongations ».
- [ ] **3.2 Barres de PV** au-dessus des bâtiments et des unités (WidgetComponent en espace écran), couleur de l'équipe.
- [ ] **3.3 Glisser-déposer** des cartes depuis le HUD vers le terrain, avec la zone de déploiement surlignée pendant le glisser.
- [ ] **3.4 Menus** : accueil (Solo contre IA, Héberger, Rejoindre, Deck, Quitter), écran de choix du deck (8 cartes parmi celles disponibles, sauvegardé en `SaveGame`), écran de fin (victoire ou défaite, couronnes, Rejouer).

**Terminé quand** : on peut lancer, jouer et terminer une partie sans la console ni l'éditeur.

---

## Phase 4 — Modèles, animations et effets

### 4.1 Unités 3D (dans Blender, même style détaillé que l'arène)
- [ ] Concevoir les 6 unités de la phase 1.3 (designs originaux), en version bleue et rouge via un paramètre de matériau plutôt qu'en doublant les modèles.
- [ ] Modéliser dans Blender (fichiers `Arena/Unites/<Unite>.blend`), rigger avec Rigify (ou squelette compatible UE Mannequin pour les humanoïdes), puis animer : Idle, Marche, Attaque, Mort, Apparition.
- [ ] Exporter en FBX squelettique, importer dans `/Game/Arene/Unites/<Unite>/`, créer l'AnimBlueprint et le brancher sur `AArenaUnit` (vitesse, événement d'attaque via AnimNotify pour synchroniser les dégâts).
- [ ] Tireurs visibles au sommet des tours (sur `SLOT_Unite_*`) et du fort.

### 4.2 Effets (Niagara) et sons
- [ ] Apparition d'unité, projectiles (flèche, boule de feu), impacts, mort d'unité.
- [ ] Bâtiment endommagé puis détruit : explosion, débris, nuage de poussière, fumée persistante sur la ruine, tremblement de caméra.
- [ ] Sorts : zone visible au sol avant l'impact.
- [ ] Sons : impacts, apparitions, destruction, ambiance de l'arène, musique, signal des prolongations. Utiliser des sources libres de droits uniquement.

### 4.3 Habillage de l'arène
- [ ] Remplacer les couleurs unies des `MI_*` par de vraies textures (pierre, bois, tuiles, herbe). Ajouter des paramètres de texture aux matériaux maîtres, en conservant la teinte par vertex color.
- [ ] Herbe animée par le vent (World Position Offset), eau avec normales animées, scintillement des flammes.
- [ ] Post-process : étalonnage, léger bloom, vignette.

**Terminé quand** : plus aucune forme provisoire en jeu, et chaque action a un retour visuel et sonore.

---

## Phase 5 — Contenu et équilibrage
- [ ] Porter le jeu à 15–20 cartes : nouveaux rôles (soigneur, invocateur, bâtiment défensif posable, sort de gel…).
- [ ] Organiser des sessions de test avec les amis et ajuster les coûts et statistiques des Data Assets, en notant les changements dans le Journal.
- [ ] Optionnel : déblocage des cartes et cosmétiques (skins de tours, emotes), statistiques des parties.
- [ ] Optionnel : une deuxième arène réutilisant la même structure (slots, zones, chemins, tags).

---

## Phase 6 — Distribution
- [ ] Packager en Windows Shipping (Platforms > Windows > Package Project) et tester l'exécutable hors éditeur.
- [ ] Distribuer en zip ou sur une page itch.io privée accessible par lien. Prévoir un numéro de version affiché dans le menu et un moyen simple de diffuser les mises à jour.

---

## Journal
<!-- Une ligne par session : date — ce qui a été fait — décisions — problèmes ouverts -->
- 2026-10-09 — Arène, bâtiments (intact / endommagé / ruine), collisions, navigation et repères de gameplay créés dans Blender, exportés et importés dans Unreal via `import_unreal.py`. Prochaine étape : Phase 0.
- 2026-10-09 — Vérification de la Phase 0 (0.1, 0.3, 0.4 faites ; 0.2, 0.5, 0.6 restent à faire).
  - 0.1 : projet `Anera.uproject` (racine du dépôt), UE 5.8 (`C:/Program Files/Epic Games/UE_5.8`, associé par GUID), niveau `/Game/Arene/Maps/Arene`. `GameDefaultMap` pointe encore sur `/Engine/Maps/Templates/OpenWorld` et `EditorStartupMap` n'est pas défini.
  - 0.3 : plugins Model Context Protocol et All Toolsets activés, `bAutoStartServer=True`, `.mcp.json` présent, `list_toolsets` répond. Aucun toolset Live Coding listé (à revérifier une fois le module C++ créé).
  - 0.4 : axe camp J → camp A = **−Y** (camp J côté +Y). « Gauche » et « Droite » sont des noms globaux vus depuis J : Gauche = −X, Droite = +X. Pour A (vue retournée), sa tour « Gauche » apparaît donc à droite de son écran.
    - Slots (x, y, z) : `Tour_Gauche_J` (−550, 900, 52), `Tour_Droite_J` (550, 900, 52), `Tour_Gauche_A` (−550, −900, 52), `Tour_Droite_A` (550, −900, 52), `Fort_J` (0, 1350, 58), `Fort_A` (0, −1350, 58). Slots J orientés yaw −90 (vers A), slots A yaw +90. `SLOT_Unite_Tour_*` à z = 366, `SLOT_Unite_Fort_*` à (0, ±1320, 338).
    - Zones de déploiement centrées en y = ±860. Chemins : 8 points (`_00` à `_07`) par équipe et par voie, du pont (y = 0, z = 51) à la tour ennemie puis au fort ennemi.
    - 118 acteurs `ArenaImport`. Les 6 bâtiments ont leurs 3 états, plus les flammes et les lumières.
  - Problèmes ouverts :
    - Pas de dossier `Source/`, et ni Visual Studio ni l'outillage MSVC ne sont installés : 0.2 est bloquée.
    - Pas de dépôt git (git 2.55 et git-lfs 3.7 sont installés).
    - `Camera_Arene` est encore en 9:16 (AspectRatio 0,5625, FOV horizontal 42,6°, `AutoActivateForPlayer = Player0`). En 16:9, le cadre irait seulement d'environ y = +900 à y = −1000, et les deux forts sortiraient de l'écran.
    - World Settings sans GameMode (normal avant la phase 1).
- 2026-10-09 — 0.2 et 0.5 faites.
  - 0.2 : module C++ **`Anera`** (c'est le `<Module>` de la suite, macro d'export `ANERA_API`), cibles `Anera` et `AneraEditor` (UE 5.8.3, Visual Studio Community 2026). Il contient un `AArenaGameMode` vide, et l'éditeur charge bien `/Script/Anera.ArenaGameMode`. Compilation hors éditeur : `Build.bat AneraEditor Win64 Development -Project=...`. Aucun toolset Live Coding n'est exposé par MCP (dans l'éditeur : Ctrl+Alt+F11).
  - 0.5 : dépôt git (branche `main`, remote `origin` = GitHub `TheProdigy27/Anera`, **public**), `.gitignore` Unreal, Git LFS pour `*.uasset` / `*.umap`. Rien n'est poussé pour l'instant.
  - Données perso retirées avant le premier push : prénom et chemins locaux (CLAUDE.md, PLAN.md), ainsi que le chemin, la date et le MD5 des FBX sources dans les 46 StaticMesh (données d'import effacées en commandlet Python). **Relancer `import_unreal.py` les remettrait.**
  - « Ce projet a été réalisé avec une version différente » : l'association moteur était un GUID du registre, effacé par Unreal comme doublon de l'installation launcher. Corrigé avec `"EngineAssociation": "5.8"`.
  - `EditorStartupMap` et `GameDefaultMap` pointent maintenant sur `/Game/Arene/Maps/Arene`.
  - Problèmes ouverts : 0.6 (caméra 16:9) ; erreur au démarrage « Asset manager settings do not include a rule for assets of type GameFeatureData » (plugin GameFeatures, chargé avec les toolsets MCP).
