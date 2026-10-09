# Projet : jeu d'arène 1v1 (Unreal Engine 5, PC)

Jeu PC privé, joué entre amis, au gameplay de type « lane battle » en 1 contre 1 : on pose des cartes (unités et sorts) qui avancent sur deux couloirs pour détruire les tours puis le fort adverse. Univers et unités 100 % originaux : ne jamais reprendre de noms, personnages ou visuels d'un jeu existant.

Le plan de développement détaillé est dans **`PLAN.md`** (à la racine du projet). Le lire en début de session, reprendre à la première tâche non cochée et cocher les tâches terminées.

## Environnement
- Moteur : Unreal Engine 5.x, PC Windows uniquement. Pas de mobile, pas de store.
- Développeur : à l'aise en Rust et en TypeScript, débutant sur Unreal. Expliquer brièvement les concepts propres à Unreal (réplication, GameMode/GameState, UPROPERTY…) la première fois qu'ils apparaissent.
- Langue : commentaires, noms d'assets et textes du jeu en français. Identifiants C++ en anglais.
- Unreal MCP : le plugin `unreal-mcp` donne accès à l'éditeur ouvert (voir PLAN.md, étape 0). Il sert à inspecter le niveau, créer des Blueprints, Widgets et Data Assets, et lancer Live Coding.
- Blender MCP (optionnel) : les sources 3D sont dans Blender (voir « Sources 3D »). Il sert à modéliser les unités et les props.

## Ce qui existe déjà dans Unreal (importé par script, ne pas casser)
- Assets :
  - `/Game/Arene/Meshes` : 46 StaticMesh. Nanite est activé sur le décor, la végétation et les bâtiments.
  - `/Game/Arene/Materiaux` :
    - 4 matériaux maîtres : `M_Arene_Opaque`, `M_Arene_Sol`, `M_Arene_Flamme`, `M_Arene_Eau`.
    - Un `MI_<NomBlender>` par matériau.
    - Les nuances viennent des vertex colors : R = variation par brique ou tuile, G = terre et B = sable (sur le sol uniquement).
- Niveau : tous les acteurs importés portent le tag `ArenaImport` et sont rangés dans les dossiers de l'Outliner `Arene/...`.
  - `Arene/Decor` : sol, terrain, eau, ponts, remparts, socles, braseros, végétation, pavés. Pas de collision.
  - `Arene/Batiments/<Groupe>` : pour chaque bâtiment, 3 StaticMeshActors (`SM_<Groupe>_Intact`, `SM_<Groupe>_Endommage`, `SM_<Groupe>_Ruine`), plus parfois `..._Flammes` et des PointLights.
    - Groupes : `Tour_Gauche_J`, `Tour_Droite_J`, `Tour_Gauche_A`, `Tour_Droite_A`, `Fort_J`, `Fort_A`.
    - Tags : `batiment:<Groupe>`, `etat:Intact|Endommage|Ruine`, `groupe:<Groupe>`.
    - Seul l'état Intact est visible. Les autres sont cachés en jeu (`SetActorHiddenInGame`).
    - Les collisions simplifiées (UCX) sont dans les meshes de bâtiments.
  - `Arene/Gameplay` :
    - `SM_Collisions_Arene` : collisions invisibles (murs, rivière hors ponts, socles, braseros) et sol marchable.
    - `NavMesh_Arene` : un NavMeshBoundsVolume.
    - `Camera_Arene` : la caméra de jeu, vue de dessus inclinée, cadrée en portrait. À adapter au 16:9 PC (voir PLAN.md).
    - `Slots/` : des TargetPoints.
      - `SLOT_Tour_<Gauche|Droite>_<J|A>` et `SLOT_Fort_<J|A>` : emplacements des bâtiments.
      - `SLOT_Unite_<Groupe>` : emplacement d'une unité défensive au sommet d'un bâtiment.
      - Tags `slot`, `type:tour|fort|unite_tour|unite_fort`, `equipe:joueur|adverse`, `parent:<Groupe>`.
    - `Zones/` : `ZONE_Deploiement_J` et `ZONE_Deploiement_A` (TriggerBox), chacune couvre la moitié de terrain de son camp. Tags `zone`, `type:zone_deploiement`, `equipe:...`.
    - `Chemins/` : `CHEMIN_IA_<J|A>_<Gauche|Droite>_NN` (TargetPoints numérotés), qui vont du pont à la tour ennemie puis au fort ennemi. Tags `chemin:<nom>`, `index:N`, `voie:gauche|droite`, `equipe:...`.
  - `Arene/Lumieres` : les PointLights des braseros et lanternes.
- Convention d'équipe : `J` = joueur 1 (hôte), `A` = adversaire (joueur 2).

## Dimensions (en unités Unreal, 1 uu = 1 cm)
- Terrain jouable : 1800 × 3200 uu. La rivière fait environ 240 uu de large au centre, avec 2 ponts sur les couloirs (axe des couloirs à ±550 uu du centre).
- Tours à ±550 uu latéralement et à 900 uu du centre. Forts sur l'axe central, à 1350 uu du centre.
- **Ne pas supposer l'orientation des axes** : l'import a recalé les axes automatiquement. Lire les positions réelles des acteurs `SLOT_*` (via MCP ou `GetAllActorsWithTag`) avant d'écrire du code qui en dépend. Le code de gameplay doit **toujours** trouver les emplacements par tags, jamais par coordonnées en dur.

## Sources 3D (sur le PC du développeur, hors dépôt)
- Fichier maître : `%USERPROFILE%\Documents\Anera\Blender Assets\Arena\Arena_1.blend`.
- Fichiers par élément : `...\Arena\Assets\` (`Arene.blend`, `Tour_Joueur.blend`, `Tour_Adverse.blend`, `Fort_Joueur.blend`, `Fort_Adverse.blend`).
- Export Unreal : `...\Arena\Unreal_Export\` (`FBX/`, `scene.json`, `import_unreal.py`). Le script est ré-exécutable : il supprime puis recrée les acteurs tagués `ArenaImport`. **Il effacerait toute modification manuelle faite sur ces acteurs.**

## Règles de travail
1. Lire `PLAN.md`, annoncer la tâche visée, puis travailler par petites étapes testables.
2. Avant toute modification en masse (assets ou niveau), demander au développeur d'enregistrer (Ctrl+Shift+S), ou enregistrer via MCP. Enregistrer à nouveau après.
3. Ne pas modifier ni supprimer les acteurs tagués `ArenaImport`. Les piloter depuis le code (visibilité, collisions), ou les dupliquer.
4. Logique de jeu en C++ (module du projet), contenu et réglages en Blueprints et Data Assets dérivés des classes C++.
5. Penser multijoueur dès le départ : le serveur fait autorité (placement de carte via Server RPC, élixir et PV calculés côté serveur, variables répliquées). Le solo contre l'IA tourne sur le même code.
6. Après une modif C++ : compiler avec Live Coding (via MCP si disponible), lire les erreurs, corriger, puis tester en PIE.
7. Toujours vérifier le résultat d'un appel MCP (succès explicite) avant de continuer.
8. Pas de noms, personnages ou visuels repris d'un jeu existant.
9. En fin de session : cocher les tâches faites dans `PLAN.md` et ajouter une ligne au « Journal » en bas du fichier.
