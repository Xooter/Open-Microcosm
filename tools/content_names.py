"""Nombres del contenido y de la configuración: español (formato viejo) → inglés.

Es la única tabla de la migración: la usa tools/migrate_content.py para los datos y de acá salen
los alias de los ids viejos (`legacy_ids`) y de las claves viejas que el programa todavía lee.
Solo nombres de máquina (claves, ids, valores con nombre); el texto que lee una persona (nombre,
descripción, fuente, definiciones, pasos) no se traduce acá: lo traduce assets/lang.
"""

# --- Tablas y claves ---------------------------------------------------------------------------
KEYS = {
    # manifiesto
    "paquete": "pack", "nombre": "name", "autor": "author", "licencia": "license",
    "programa_minimo": "min_program", "orden": "order",
    # tablas de definiciones
    "ser": "being", "plan": "body_plan", "mineral": "mineral", "bioma": "biome", "medio": "medium",
    "recipiente": "vessel", "placa": "plate", "leccion": "lesson", "reactivo": "reagent",
    "termino": "term", "fisiologia": "physiology", "escena": "scene",
    # ser
    "descripcion": "description", "etiquetas": "tags", "locomocion": "locomotion",
    "alimentacion": "feeding", "velocidad_relativa": "relative_speed", "presas": "prey",
    "presas_boca": "mouth_prey", "huye_de": "flees_from", "quimiocinesis": "chemokinesis",
    "trampa_acida": "acid_trap", "fototaxis": "phototaxis",
    "fotosintesis_relativa": "relative_photosynthesis", "divisiones_por_ciclo": "fissions_per_cycle",
    "enquista": "encysts", "conjugacion": "conjugation", "cantidad_inicial": "initial_count",
    "sesil": "sessile", "largo_tallo": "stalk_length", "espasmonema": "spasmoneme", "camina": "walks",
    "alcance_tentaculos": "tentacle_reach", "alcance_cuello": "neck_reach",
    "manipulacion_horas": "handling_hours", "cloroplastos": "chloroplasts",
    "desliza_por_rafe": "raphe_glider", "auxotrofa_b12": "b12_auxotroph", "frustula": "frustule",
    "filamento": "filament", "celulas_filamento": "filament_cells",
    "fragmentacion_por_hora": "fragmentation_per_hour", "zoosporas": "zoospores",
    "colonias_hijas": "daughter_colonies", "celulas_colonia": "colony_cells",
    "madre_muere_al_liberar": "mother_dies_at_release", "masa_seca_relativa": "relative_dry_mass",
    "gemacion": "budding", "anaerobia": "anaerobe", "boca_posterior": "posterior_mouth",
    "ciclosis": "cyclosis", "ph": "ph", "temperatura": "temperature", "salinidad_max": "max_salinity",
    "reserva": "storage", "pared": "wall", "genoma": "genome", "forma": "shape",
    "apariencia": "appearance", "celula": "cell",
    # genes
    "tamano": "size", "motor": "motor", "mitocondrias": "mitochondria", "lisosomas": "lysosomes",
    "tricocistos": "trichocysts", "coraza": "armor", "rigidez": "rigidity", "apareamiento": "mating_type",
    # [ser.celula]
    "adherente": "adherent", "radio_redondeada": "rounded_radius",
    "inhibicion_contacto_locomocion": "contact_inhibition_locomotion",
    "inhibicion_contacto_proliferacion": "contact_inhibition_proliferation",
    "suero_minimo": "min_serum", "apoptosis_sin_suero_h": "apoptosis_without_serum_h",
    "se_divide": "divides", "quimiotaxis_fmlp": "fmlp_chemotaxis",
    # [ser.apariencia] y [ser.forma]
    "estilo": "style", "color_plastos": "plastid_color", "semieje_mayor": "semi_major_axis",
    "semieje_menor": "semi_minor_axis", "memoria_forma": "shape_memory", "color": "color",
    "nucleo": "nucleus", "cuadratura": "squareness", "afinamiento": "taper", "espesor": "thickness",
    # fisiología
    "crecimiento_maximo": "max_growth", "p_media_saturacion": "p_half_saturation",
    "si_media_saturacion": "si_half_saturation", "rapidez_migracion": "migration_speed",
    "persistencia": "persistence", "fase_g1": "phase_g1", "fase_s": "phase_s", "fase_g2": "phase_g2",
    "fase_m": "phase_m", "vida": "lifespan", "confluencia": "confluence",
    "glucosa_vmax": "glucose_vmax", "glucosa_km": "glucose_km", "glutamina_vmax": "glutamine_vmax",
    "glutamina_km": "glutamine_km", "lactato_por_glucosa": "lactate_per_glucose",
    "o2_consumo": "o2_uptake", "ingestion_maxima": "max_ingestion", "aclaramiento": "clearance",
    "valor": "value", "unidad": "unit", "fuente": "source", "estimacion": "estimate",
    # planes corporales
    "morfologia": "morphology", "estructuras": "structures", "tipo": "kind",
    "disposicion": "arrangement", "largo": "length", "ancho": "width", "por_um3": "per_um3",
    "cantidad": "count", "campo_claro": "brightfield",
    # minerales
    "nombre_grano": "grain_name", "radio": "radius", "diferencia_densidad": "density_difference",
    "opacidad": "opacity", "flocula": "flocculates",
    # biomas
    "bacterias": "bacteria", "detritos": "detritus", "filamentos_fijadores": "nitrogen_fixing_filaments",
    "filamentos_oscillatoria": "oscillatoria_filaments",
    "cianobacterias_temperatura": "cyanobacteria_temperature", "saprolegnia": "saprolegnia",
    "grupos_quitridios": "chytrid_clusters", "allomyces": "allomyces", "liquenes": "lichens",
    "physarum": "physarum", "manchas": "patchiness", "peso_base": "base_weight", "pesos": "weights",
    "sedimento": "sediment", "granos": "grains", "grano_radio": "grain_radius",
    # medio (también los parámetros medium.*)
    "salinidad": "salinity", "luz": "light", "intercambio_o2": "o2_exchange", "anoxico": "anoxic",
    "alcalinidad": "alkalinity", "fosfato": "phosphate", "nitrogeno": "nitrogen", "silice": "silicate",
    "hierro": "iron", "vitamina_b12": "vitamin_b12", "b12_fuente": "b12_source",
    "b12_degradacion": "b12_decay", "disolucion_frustulas": "frustule_dissolution", "fagos": "phages",
    "fagos_inactivacion": "phage_inactivation", "toxinas_degradacion": "toxin_decay",
    "ahl_vida_media": "ahl_half_life", "aporte_organico": "organic_inflow",
    "descomposicion": "decomposition", "detritos_entrantes": "detritus_inflow",
    "consumo_nutrientes": "nutrient_uptake", "polvo": "dust", "glucosa": "glucose",
    "glutamina": "glutamine", "suero": "serum", "rojo_fenol": "phenol_red",
    "pka_carbonato": "carbonate_pka", "co2_aire": "air_co2", "osmolitos": "osmolytes",
    "perfusion": "perfusion",
    # medios de cultivo y recipientes
    "composicion": "composition", "area_cm2": "area_cm2", "volumen_ml": "volume_ml",
    # placas preparadas
    "campo_um": "field_um", "ambiente": "environment", "siembra": "seeding", "tejido": "tissue",
    "fijado": "fixed", "tincion": "stain", "celda_um": "cell_um", "eritrocitos_cm2": "erythrocytes_cm2",
    "plaquetas_cm2": "platelets_cm2", "bacterias_cm2": "bacteria_cm2", "densidad_cm2": "density_cm2",
    "contenido": "content", "largo_um": "length_um", "ancho_um": "width_um", "pared_um": "wall_um",
    "osmolaridad_jugo": "sap_osmolarity", "inactivo": "inactive", "equilibrio_s": "equilibration_s",
    "fases_mitosis": "mitotic_phases",
    # lecciones
    "titulo": "title", "nivel": "level", "objetivo": "goal", "cultivo": "culture", "preparar": "setup",
    "solucion": "solution", "solucion_horas": "solution_hours", "fijados": "locked",
    "controles": "controls", "acciones": "actions", "paso": "step", "texto": "text",
    "pregunta": "question", "opciones": "options", "correcta": "correct", "explicacion": "explanation",
    "prediccion": "prediction", "magnitud": "quantity", "clave": "key", "op": "op", "motivo": "reason",
    "evento": "event",
    # reactivos
    "mecanismo": "mechanism", "blancos": "targets", "depende_del_crecimiento": "growth_dependent",
    "difusion": "diffusion", "farmacodinamia": "pharmacodynamics", "cim_publicada": "published_mic",
    "psi_max": "psi_max", "psi_min": "psi_min", "zmic": "zmic", "kappa": "kappa",
    # glosario
    "definicion": "definition", "alias": "alias", "relacionados": "related",
    # experimentos
    "experimento": "experiment", "replicas": "replicates", "horas": "hours", "muestreo_h": "sample_h",
    "hilos": "threads", "programa": "program", "eventos": "events", "factor": "factor",
    "valores": "values",
}
# Claves que conservan su nombre (ya en inglés o sin traducción).
SAME_KEYS = {"id", "version", "seed", "co2", "kappa"}

# --- Valores con nombre -------------------------------------------------------------------------
VALUES = {
    # locomoción y alimentación
    "cilios": "cilia", "flagelo": "flagellum", "pseudopodo": "pseudopod", "arrastre": "crawl",
    "ninguna": "none", "surco_oral": "oral_groove", "polo_anterior": "anterior_pole",
    "fagocitosis": "phagocytosis", "raptorial": "raptorial", "tentaculos": "tentacles", "cuello": "neck",
    "osmotrofia": "osmotrophy",
    # reservas y paredes
    "almidon": "starch", "glucogeno": "glycogen", "paramilon": "paramylon",
    "crisolaminarina": "chrysolaminarin", "celulosa": "cellulose", "glucano_levadura": "yeast_glucan",
    # estilos de dibujo de los seres
    "ciliado": "ciliate", "flagelado": "flagellate", "ameboide": "amoeboid",
    "rostro_de_flagelos": "flagellar_rostrum", "cinturones_y_proboscide": "girdles_and_proboscis",
    "cinta_helicoidal": "helical_ribbon", "placa_axial": "axial_plate",
    "plastos_estrellados": "stellate_plastids", "plasto_reticulado": "reticulate_plastid",
    "frustula": "frustule", "colonia_esferica": "spherical_colony", "colonia_plana": "flat_colony",
    "corona_peristomal": "peristomal_crown", "corona_de_membranelas": "membranelle_crown",
    "tentaculos_capitados": "capitate_tentacles", "cirros": "cirri", "cuello_extensible": "extensible_neck",
    "dos_flagelos": "two_flagella", "levadura": "yeast", "celula_adherida": "adherent_cell",
    "neutrofilo": "neutrophil", "protoplasto_vegetal": "plant_protoplast", "hoja_elodea": "elodea_leaf",
    "celula_escamosa": "squamous_cell", "linfocito": "lymphocyte", "monocito": "monocyte",
    "cocoide": "coccoid", "colonia_de_placa": "plate_colony", "colonia_compacta": "compact_colony",
    "colonia_hueca": "hollow_colony",
    # estilos de los minerales
    "floculo": "floc", "grano_anguloso": "angular_grain", "escama": "flake", "grano_opaco": "opaque_grain",
    "globulo_refringente": "refractile_globule",
    # etiquetas
    "alga": "alga", "alga_verde": "green_alga", "ameba": "amoeboid", "bacterivoro": "bacterivore",
    "celula_animal": "animal_cell", "celula_humana": "human_cell", "celula_vegetal": "plant_cell",
    "colonia": "colonial", "desmidia": "desmid", "diatomea": "diatom", "dinoflagelado": "dinoflagellate",
    "epidermis": "epidermis", "epitelio": "epithelium", "euglenido": "euglenid", "fagocito": "phagocyte",
    "fibroblasto": "fibroblast", "filamentosa": "filamentous", "fotobionte": "photobiont",
    "fototrofa": "phototroph", "hongo": "fungus", "hospeda_quitridios": "chytrid_host",
    "leucocito": "leukocyte", "linea_tumoral": "tumor_line", "marina": "marine", "protista": "protist",
    "queratinocito": "keratinocyte", "simbionte_obligado": "obligate_symbiont",
    "simbionte_termita": "termite_symbiont", "suctorio": "suctorian", "tecameba": "testate_amoeba",
    # estructuras de los planes corporales
    "membrana_plasmatica": "plasma_membrane", "membrana_externa": "outer_membrane",
    "peptidoglicano": "peptidoglycan", "pared": "cell_wall", "capsula": "capsule", "nucleoide": "nucleoid",
    "ribosomas": "ribosomes", "pili": "pili", "nucleo": "nucleus", "nucleolo": "nucleolus",
    "mitocondria": "mitochondrion", "hidrogenosoma": "hydrogenosome", "cloroplasto": "chloroplast",
    "pirenoide": "pyrenoid", "estigma": "eyespot", "reticulo_endoplasmico": "endoplasmic_reticulum",
    "golgi": "golgi", "vacuola_contractil": "contractile_vacuole", "vacuola_digestiva": "food_vacuole",
    "lisosoma": "lysosome", "granulo_reserva": "storage_granule", "capside": "capsid",
    "genoma_viral": "viral_genome", "envoltura_viral": "viral_envelope", "cola": "tail",
    "placa_basal": "baseplate", "fibras_cola": "tail_fibers",
    "simulada": "simulated", "declarada": "declared", "envoltura": "envelope", "interior": "interior",
    "polar": "polar", "peritrica": "peritrichous", "perinuclear": "perinuclear", "anterior": "anterior",
    "bacilo": "rod", "vibrio": "vibrio", "eucariota": "eukaryote", "fago_cola": "tailed_phage",
    "icosaedrica": "icosahedral", "helicoidal": "helical",
    # tejidos y placas
    "hileras": "rows", "poligonal": "polygonal", "filas": "files", "rompecabezas": "jigsaw",
    "protoplasto": "protoplast", "mitosis": "mitosis", "estomas": "stomata",
    "preparado": "slide",
    # unidades de la fisiología y de las predicciones
    "1/dia": "1/day", "dia": "day", "bacterias/h": "bacteria/h", "bacterias/min": "bacteria/min",
    "fraccion": "fraction",
    # condiciones de las lecciones y solutos
    "tiempo_h": "time_h", "poblacion": "population", "bacterias": "bacteria",
    "concentracion": "concentration", "plasmolisis": "plasmolysis", "hemolisis": "hemolysis",
    "cerca_centro": "near_center", "fosfato": "phosphate", "nitrogeno": "nitrogen", "silice": "silicate",
    "hierro": "iron", "organica": "organic",
    # reactivos
    "farmacodinamico": "pharmacodynamic",
}
# Valores que dependen de la clave que los tiene.
VALUES_BY_KEY = {
    "pared": {"silice": "silica", "ninguna": "none"},
    "plan": {"ameba": "amoeba"},
    "tipo": {"cultivo": "cell_culture"},
}

# --- Ids ----------------------------------------------------------------------------------------
IDS = {
    "beings": {
        "fibroblasto_dermico_humano": "human_dermal_fibroblast", "neutrofilo": "neutrophil",
        "celula_epidermica_cebolla": "onion_epidermal_cell", "celula_hoja_elodea": "elodea_leaf_cell",
        "celula_escamosa_bucal": "buccal_squamous_cell", "linfocito": "lymphocyte", "monocito": "monocyte",
    },
    "body_plans": {
        "ciliado": "ciliate", "euglenido": "euglenid", "ameba": "amoeba",
        "clorofita_flagelada": "flagellate_chlorophyte", "criptofita": "cryptophyte",
        "euglenido_incoloro": "colorless_euglenid", "dinoflagelado": "dinoflagellate",
        "alga_verde": "green_alga", "diatomea": "diatom", "levadura": "yeast",
        "parabasalido": "parabasalid", "celula_humana_adherida": "adherent_human_cell",
        "neutrofilo": "neutrophil", "celula_vegetal": "plant_cell",
        "celula_vegetal_cloroplastos": "plant_cell_chloroplasts", "celula_escamosa": "squamous_cell",
        "leucocito": "leukocyte",
    },
    "minerals": {
        "arcilla": "clay", "cuarzo": "quartz", "oxido_hierro": "iron_oxide", "azufre": "sulfur",
        "frustula": "frustule",
    },
    "biomes": {
        "charco": "puddle", "estanque": "pond", "musgo": "moss", "intestino_termita": "termite_gut",
        "fuente_termal": "hot_spring", "hielo_marino": "sea_ice",
    },
    "media": {
        "dmem_alta_glucosa": "dmem_high_glucose", "agua_destilada": "distilled_water",
        "solucion_salina_09": "saline_0_9",
    },
    "vessels": {
        "placa_35mm": "dish_35mm", "placa_60mm": "dish_60mm", "placa_100mm": "dish_100mm",
        "pocillo_6": "well_6", "pocillo_24": "well_24", "pocillo_96": "well_96", "frasco_t25": "flask_t25",
        "portaobjetos": "slide", "tubo_gause": "gause_tube", "tubo_ensayo_10": "test_tube_10",
        "microquimiostato": "microchemostat",
    },
    "plates": {
        "cultivo_dmem": "dmem_culture", "epidermis_cebolla": "onion_epidermis", "hoja_elodea": "elodea_leaf",
        "papa_lugol": "potato_lugol", "raiz_cebolla": "onion_root_tip", "epidermis_hoja": "leaf_epidermis",
        "mucosa_bucal": "buccal_mucosa", "frotis_sangre": "blood_smear", "sangre_viva": "live_blood",
    },
    "lessons": {
        "plasmolisis_cebolla": "onion_plasmolysis", "fragilidad_osmotica": "osmotic_fragility",
        "gause_competencia": "gause_competition", "tilman_quimiostato": "tilman_chemostat",
    },
    "reagents": {"ampicilina": "ampicillin", "tetraciclina": "tetracycline"},
    "glossary": {
        "numero_reynolds": "reynolds_number", "quimiotaxis": "chemotaxis", "fototaxis": "phototaxis",
        "reaccion_evitacion": "avoidance_reaction", "quimiostato": "chemostat", "r_estrella": "r_star",
        "exclusion_competitiva": "competitive_exclusion", "respuesta_funcional": "functional_response",
        "aclaramiento": "clearance", "plasmolisis": "plasmolysis", "hemolisis": "hemolysis",
        "vacuola_contractil": "contractile_vacuole", "vacuola_digestiva": "food_vacuole",
        "fagocitosis": "phagocytosis", "citostoma": "cytostome", "conjugacion": "conjugation",
        "quiste": "cyst", "biopelicula": "biofilm", "fago": "phage", "cim": "mic", "ci50": "ic50",
        "ley_henry": "henry_law", "respiracion": "respiration", "fermentacion": "fermentation",
        "fotosintesis": "photosynthesis", "heterocisto": "heterocyst", "auxotrofia": "auxotrophy",
        "inhibicion_contacto": "contact_inhibition", "punto_restriccion": "restriction_point",
        "campo_claro": "brightfield",
    },
    "stains": {"orceina_acetica": "acetic_orcein", "tincion": "stain"},
}

# Qué tipo de id nombra cada clave (para traducir sus valores).
ID_KEYS = {
    "plan": "body_plans", "bioma": "biomes", "placa": "plates", "medio": "media", "recipiente": "vessels",
    "ser": "beings", "leccion": "lessons", "relacionados": "glossary", "tincion": "stains",
}
# Claves cuyos valores son ids de seres o etiquetas (dietas y huidas).
BEING_REF_KEYS = {"presas", "presas_boca", "huye_de"}
# Tablas cuyas claves son ids: [bioma] pesos (seres) y sedimento (minerales).
ID_TABLE_KEYS = {"pesos": "beings", "sedimento": "minerals"}
# Las claves con texto para leer: sus valores no se tocan.
TEXT_KEYS = {
    "nombre", "descripcion", "fuente", "titulo", "objetivo", "texto", "explicacion", "motivo",
    "definicion", "termino", "nombre_grano", "alias", "opciones", "autor", "licencia", "version",
    "nivel", "programa_minimo",
}

# --- Parámetros del registro (game/parameters) --------------------------------------------------
PARAM_GROUPS = {
    "medio": "medium", "bacterias": "bacteria", "hongos": "fungi", "physarum": "physarum",
    "ambiente": "environment", "tiempo": "time", "agua": "water", "xpbd": "xpbd", "lod": "lod",
    "metabolismo": "metabolism",
}
PARAM_NAMES = {
    "quimiotaxis": "chemotaxis", "mantencion_ayuno": "starvation_maintenance", "adhesion": "adhesion",
    "desprendimiento": "detachment", "absorcion": "absorption", "mantenimiento": "maintenance",
    "hambre_a_esporas": "starvation_to_spores", "exoenzimas": "exoenzymes",
    "allomyces_despierta": "allomyces_wakeup", "liquen_azucar": "lichen_sugar",
    "liquen_fosforo": "lichen_phosphorus", "frente": "front", "adaptacion": "adaptation", "gamma": "gamma",
    "cambio": "turnover", "evita_baba": "slime_avoidance", "evita_luz": "light_avoidance",
    "retraccion": "retraction", "hambre_a_esclerocio": "starvation_to_sclerotium",
    "escala_biologica": "bio_scale", "espesor_pelicula": "film_thickness", "subpasos": "substeps",
    "flexion": "bending", "area": "area", "aristas": "edges", "iteraciones": "iterations",
    "pasadas_contacto": "contact_passes", "cada": "stride", "costo_nado": "swim_cost",
    "rendimiento": "yield", "o2_media_saturacion": "o2_half_saturation",
    "penalidad_fermentacion": "fermentation_penalty", "fotosintesis_maxima": "max_photosynthesis",
    "exudacion": "exudation", "division": "division", "costo_division": "division_cost",
    "costo_division_controlado": "controlled_division_cost", "fraccion_muerte": "death_fraction",
    "cuota_maxima": "max_quota", "cuota_p": "p_quota", "n_media_saturacion": "n_half_saturation",
    "cuota_n": "n_quota", "cuota_si": "si_quota", "cuota_fe": "fe_quota", "cuota_b12": "b12_quota",
    "fe_media_saturacion": "fe_half_saturation", "b12_media_saturacion": "b12_half_saturation",
}


def param_key(key):
    """'medio.alcalinidad' → 'medium.alkalinity' (None si no es una clave de parámetro)."""
    if "." not in key:
        return None
    group, name = key.split(".", 1)
    if group not in PARAM_GROUPS:
        return None
    return PARAM_GROUPS[group] + "." + PARAM_NAMES.get(name, KEYS.get(name, name))


# --- Escenas de cultivo y programas -------------------------------------------------------------
CULTURE_KEYS = {
    "cultivo_version": "culture_version", "nombre": "name", "recipiente": "vessel", "medio": "medium",
    "temperatura": "temperature", "luz": "light", "seed": "seed", "programa": "program", "evento": "event",
}
CULTURE_OPS = {
    "inocular": "inoculate", "bacterias": "bacteria", "medio": "medium", "dilucion": "dilution",
    "concentracion": "concentration", "fagos": "phages", "muestra": "sample", "salida": "withdraw",
    "entrada": "receive", "inocular_densidad": "inoculate_density", "quimiostato": "chemostat",
    "medio_que_entra": "inflow_medium", "reactivo": "reagent",
}
SOLUTES = {
    "o2": "o2", "co2": "co2", "fosfato": "phosphate", "nitrogeno": "nitrogen", "silice": "silicate",
    "hierro": "iron", "b12": "b12", "organica": "organic",
}
PROGRAM_SHAPES = {"rampa": "ramp", "ciclo": "cycle", "seno": "sine", "evento": "event"}
PROGRAM_TARGETS = {
    "bacterias": "bacteria", "medio": "medium", "dilucion": "dilution", "temperatura": "temperature",
    "luz": "light",
}
PROGRAM_PREFIXES = {"agregar": "add", "inocular": "inoculate", "soluto": "solute"}
