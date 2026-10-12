# BioGame — Documento de gameplay

> Vida acuática microscópica, simulada con partículas. Células gelatinosas y semitransparentes,
> vistas como en un microscopio, que se fusionan, se dividen, comen y mueren como en la vida real.

**Cómo usar este documento:** en cada sección marcá con `[x]` la opción que más te guste (podés
marcar más de una si querés combinarlas), escribí la letra en **Favorita** y dejá tus cambios o
ideas en **Notas**. Después uso tus elecciones para ajustar el roadmap.

---

## 1. Gameplay general

- [ ] **A — Dios del portaobjetos.** Sandbox de observación e intervención: no sos una célula,
      sos el científico. Agregás nutrientes, luz, toxinas o especies, y mirás cómo reacciona el ecosistema.
- [ ] **B — Evolución celular (estilo Spore, fase célula extendida).** Controlás un organismo que
      empieza como procariota y, generación a generación, llega a eucariota, colonia y multicelular.
- [x] **C — Roguelite de supervivencia.** Cada partida es una gota de agua. Sobrevivís, te
      reproducís y morís. Parte del genoma pasa a la siguiente partida.
- [ ] **D — Gestión de colonia.** No controlás una célula sino una colonia o biofilm (tipo
      Volvox o un tapete bacteriano): asignás roles, crecés en territorio y te defendés.
- [x] **E — Linaje continuo.** Controlás una célula y, al dividirte, elegís con qué hija seguir.
      Al morir podés "saltar" a cualquier descendiente vivo. El juego termina si se extingue tu linaje.

**Favorita:** ___

**Notas / cambios / mejoras:**

> No me decido entre la C y la E

---

## 2. Jugabilidad

- [ ] **A — Control directo twin-stick.** WASD mueve (flagelo o cilios) y el mouse apunta la
      boca o los pseudópodos. Es inmediato y arcade.
- [x] **B — Locomoción física según el tipo de célula.** Cada forma de moverse se controla distinto:
      el flagelo da impulso, los cilios deslizan de forma continua y la ameba se mueve estirando
      pseudópodos con clic y arrastre. Respeta el **bajo número de Reynolds**: si dejás de moverte,
      frenás al instante (no hay inercia).
- [ ] **C — Control por quimiotaxis.** No movés la célula: liberás o detectás señales químicas y la
      célula sigue gradientes con "corre y gira" (_run & tumble_), como una bacteria real. Es más
      estratégico y menos preciso.
- [x] **D — Fagocitosis física.** Comer consiste en rodear a la presa con la membrana deformable
      hasta cerrarla en una vacuola. La presa puede escaparse si no la envolvés bien.
- [ ] **E — La célula como fábrica.** Capa de gestión interna: ATP, aminoácidos, lípidos y agua
      (osmosis). Los organelos procesan recursos y vos priorizás qué producir, como un puzzle o
      un juego de gestión.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 3. Objetivo

- [ ] **A — Fitness.** El puntaje es la cantidad de descendientes vivos. Es el objetivo darwiniano
      puro.
- [ ] **B — Escalar en complejidad.** Hay hitos: procariota → endosimbiosis (eucariota) → colonia →
      multicelular diferenciado. Cada salto desbloquea mecánicas nuevas.
- [x] **C — Dominar el ecosistema.** Ganás si tu especie llega a cierto % de la biomasa de la gota
      o charco.
- [ ] **D — Escenarios científicos.** Misiones con contexto real: "sobreviví a la sequía del
      charco (enquistate)", "resistí la Gran Oxidación", "colonizá el intestino de una termita".
- [x] **E — Enciclopedia y descubrimiento.** No hay victoria: el objetivo es descubrir, observar y
      catalogar especies, comportamientos y organelos (como una Pokédex microscópica).

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 4. Carnívoros

> En la vida real, a esta escala son depredadores de otros protistas o bacterias.

- [x] **A — Cazadores activos.** _Didinium_ (ciliado que traga paramecios enteros, más grandes
      que él) y _Lacrymaria olor_ (un "cuello" extensible que dispara hacia la presa). Son rápidos
      y agresivos.
- [x] **B — Envolvedores lentos.** _Amoeba proteus_ y _Chaos_: lentos, pero su fagocitosis
      envuelve casi cualquier cosa. Son peligrosos de cerca y fáciles de evitar de lejos.
- [x] **C — Emboscadores sésiles.** Suctorios (_Podophrya_): fijos en un lugar, con tentáculos que
      se pegan a la presa y le succionan el citoplasma. Funcionan como trampas del escenario.
- [x] **D — Gigantes multicelulares ("jefes").** Rotíferos, _Hydra_ y larvas de copépodo: hasta
      1000 veces más grandes que una bacteria. Tienen una corona ciliada que aspira todo lo que pasa
      cerca.
- [x] **E — Parásitos e invasores.** _Bdellovibrio_ (bacteria que entra en otra y la consume desde
      adentro), quitridios parásitos y fagos. El peligro es invisible hasta que es tarde.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 5. Herbívoros

> En la vida real "herbívoro" equivale a **algívoro o bacterívoro**: comen algas y bacterias.

- [x] **A — Filtradores.** _Vorticella_ y _Stentor_ crean remolinos con cilios y atrapan lo que
      pasa. Son sésiles y viven fijos a superficies; _Vorticella_ se retrae como un resorte al
      asustarse.
- [x] **B — Pastoreadores móviles.** _Paramecium_ y otros ciliados nadan comiendo bacterias y algas
      chicas. Es el "herbívoro" clásico y el arquetipo ideal del jugador inicial.
- [x] **C — Raspadores de biofilm.** _Euplotes_ (camina con patitas de cirros) y amebas con
      caparazón (_Arcella_, _Difflugia_): pastan sobre superficies y tapetes de algas.
- [x] **D — Mixótrofos.** _Euglena_: hace fotosíntesis con luz y come cuando está oscuro. Es una
      estrategia dual y el jugador decide cuál usar.
- [x] **E — Enjambres.** Comportamiento grupal emergente: se juntan, se defienden en número y
      disparan defensas como los tricocistos de _Paramecium_ (arpones que eyectan al ser atacados).

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 6. Algas

- [x] **A — Unicelulares móviles.** _Chlamydomonas_: dos flagelos y una mancha ocular (estigma)
      que la orienta hacia la luz (fototaxis). Son la presa básica y el modelo de célula vegetal.
- [x] **B — Diatomeas.** Tienen caparazón de sílice (frústula) con geometrías hermosas. Son
      "comida blindada": cuestan romperlas, se hunden y se deslizan sobre superficies.
- [x] **C — Coloniales.** _Volvox_ (esfera de miles de células con colonias hijas adentro),
      _Pediastrum_ (estrella plana) y _Scenedesmus_. Son el puente hacia la multicelularidad.
- [x] **D — Filamentosas.** _Spirogyra_, con su cloroplasto en espiral. Forman matas y laberintos:
      son terreno vivo que crece, se rompe y da refugio.
- [x] **E — Cianobacterias.** _Anabaena_ y _Nostoc_: forman filamentos con heterocistos que fijan
      nitrógeno. Producen **blooms tóxicos** como evento ambiental del nivel.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 7. Hongos

- [x] **A — Quitridios.** Zoosporas flageladas que nadan, se fijan a algas (diatomeas) y las
      parasitan. Son el hongo "móvil" y a la vez una enfermedad del ecosistema.
- [x] **B — Levaduras.** Unicelulares, se reproducen por gemación y fermentan: sin oxígeno producen
      alcohol y CO₂, que alteran el medio.
- [x] **C — Descomponedores con hifas.** _Saprolegnia_ y otros mohos acuáticos (técnicamente
      oomicetos) crecen sobre cadáveres y reciclan nutrientes. Funcionan como el "basurero" del
      ecosistema.
- [x] **D — Hongos depredadores.** _Zoophagus_ tiene hifas acuáticas con ramitas pegajosas que
      atrapan rotíferos. Es real y funciona como trampa viva.
- [ ] **E — Hongos simbiontes.** Asociación alga + hongo (estilo liquen). El jugador puede aliarse
      con un hongo: le da azúcar y recibe protección o minerales.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 8. Partículas

> Decisión técnica central: define cuánto cuesta cada célula y cuántas entran en pantalla.

- [ ] **A — Todo es partícula.** Membrana, citoplasma, organelos y agua son partículas. Es lo más
      realista y gelatinoso, y también lo más caro: pocas células grandes en pantalla.
- [x] **B — Híbrido fluido + soft-body.** El agua es un campo continuo en grilla (corrientes,
      remolinos) y las células son cuerpos blandos de partículas (anillo de membrana + presión
      interna). Es el **mejor balance entre realismo y rendimiento**.
- [x] **C — Partículas ambientales.** Nutrientes, detritos y "nieve marina" que flotan con las
      corrientes y con movimiento browniano visible. El mundo se siente vivo aunque esté vacío.
- [ ] **D — Química como campos.** O₂, CO₂, glucosa, toxinas y feromonas se difunden en grillas.
      Son invisibles por defecto y se pueden ver con un "modo tinción".
- [x] **E — Nivel de detalle (LOD).** Las células cercanas tienen miles de partículas, las lejanas
      se simplifican a pocos puntos y las fuera de pantalla se simulan de forma estadística.

**Favorita:** ___

**Notas / cambios / mejoras:**

> Primero probemos con B, luego vemos

---

## 9. Efectos visuales

- [x] **A — Membranas metaball/SDF.** Contornos suaves y orgánicos que se fusionan visualmente
      cuando dos células se tocan, con halo en el borde.
- [x] **B — Ciclosis visible.** La corriente citoplasmática real: organelos y gránulos fluyen y
      circulan dentro de la célula. Da muchísima "vida".
- [x] **C — Browniano y temblor térmico.** Las partículas chicas vibran al azar, como en un
      microscopio real. Es sutil pero inmediatamente creíble.
- [x] **D — Plano focal.** La rueda del mouse enfoca y desenfoca capas: hay profundidad falsa con
      células borrosas arriba y abajo. Se puede usar como mecánica (esconderse fuera de foco).
- [x] **E — Muerte y lisis.** La membrana revienta, el contenido se dispersa y se tiñe. Las
      vacuolas digestivas cambian de color al digerir, como el rojo neutro en el laboratorio.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 10. Reglas de vida y simbiosis

- [x] **A — Metabolismo simplificado pero real.** Glucosa → ATP. Respirar con O₂ rinde mucho;
      fermentar sin O₂ rinde poco y acidifica el medio. La luz alimenta la fotosíntesis, que libera O₂.
- [ ] **B — Endosimbiosis jugable.** Si tragás una bacteria o alga, podés **no digerirla** y
      adoptarla: así nacieron mitocondrias y cloroplastos. Ejemplo real: _Paramecium bursaria_ con
      algas _Chlorella_ adentro.
- [x] **C — Mutualismos del ecosistema.** Cianobacterias que fijan nitrógeno y alimentan algas,
      bacterias que viven en ciliados, alga + hongo. Son redes de dependencia que se pueden romper.
- [x] **D — Parasitismo y virus.** Bacteriófagos que inyectan su genoma, se replican y revientan la
      célula. Habría epidemias en el ecosistema y "inmunidad" estilo CRISPR como mejora.
- [x] **E — Quorum sensing.** Las bacterias liberan señales y, al superar un umbral de densidad,
      cambian de comportamiento en grupo: forman biofilm, se vuelven virulentas o bioluminiscentes.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 11. Estilo visual

- [x] **A — Campo claro (brightfield).** Fondo beige o blanco con células semitransparentes de
      colores tenues. Es el microscopio clásico de la escuela.
- [ ] **B — Contraste de fase.** Grises con halos brillantes alrededor de cada estructura. Muy
      "científico" y con organelos bien visibles.
- [x] **C — Campo oscuro.** Fondo negro con bordes brillantes. Es el más bello y el más legible
      para juego, porque todo resalta.
- [ ] **D — Fluorescencia.** Colores neón por tinción: núcleo azul (DAPI), mitocondrias rojas,
      membrana verde (GFP). Ideal como "modo escáner".
- [ ] **E — Ilustración estilizada.** Estilo libro de texto o acuarela: menos realista y más claro
      y artístico.

> Idea extra: los modos A–D podrían ser **lentes intercambiables** dentro del juego, donde cada uno
> revela información distinta (la fluorescencia muestra organelos de otras células, el campo
> oscuro muestra bacterias diminutas, etc.).

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 12. Reproducción

- [x] **A — Mitosis o fisión física.** La célula crece, duplica sus organelos, el anillo contráctil
      la estrangula y se parte en dos soft-bodies reales. Es el momento estrella de la simulación.
- [x] **B — Conjugación (sexual).** Dos _Paramecium_ se unen, intercambian material genético y se
      separan con rasgos mezclados. Sirve para combinar mejoras con otra célula.
- [ ] **C — Gemación.** Brota una célula hija pequeña desde la madre (levaduras, _Hydra_). Es más
      barata y más rápida, pero las hijas nacen débiles.
- [ ] **D — Quistes y esporas.** En condiciones malas (sequía, frío, sin comida) la célula se
      encierra en un quiste resistente y espera. Es una mecánica de supervivencia y guardado.
- [ ] **E — Colonial.** Como en _Volvox_: las hijas se forman **dentro** de la madre y eclosionan
      rompiéndola. Es la reproducción del modo multicelular.

**Favorita:** ___

**Notas / cambios / mejoras:**

> Cada tipo de vida tiene su propia forma de reproduccion, la del usuario es la mas costosa

---

## 13. Organelos y mejoras de personaje

- [ ] **A — Árbol de organelos clásico.** Núcleo, mitocondria (+energía), cloroplasto
      (fotosíntesis), vacuola contráctil (resistir agua dulce), lisosomas (digestión rápida),
      retículo y Golgi (producción).
- [x] **B — Organelos físicos con espacio limitado.** Los organelos ocupan lugar dentro de la
      célula: más organelos hacen una célula más grande, más lenta y más visible para depredadores.
      La ubicación también importa (por ejemplo, un flagelo atrás).
- [x] **C — Locomoción, armas y defensas.** Flagelo, cilios, pseudópodos, tricocistos, toxicistos
      (dardos venenosos), pared celular, teca o caparazón, y frústula de sílice.
- [x] **D — Genoma editable.** Cada mejora es un gen. Al reproducirte hay mutaciones aleatorias y
      podés elegir entre las hijas. Es evolución real como sistema de progresión.
- [ ] **E — Robar organelos.** Kleptoplastia real: algunos ciliados roban cloroplastos de las algas
      que comen y los usan un tiempo. Las mejoras se consiguen comiendo, no con menús.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 14. Rejugabilidad

- [x] **A — Gotas procedurales.** Cada mundo sale de una seed con temperatura, luz, pH, salinidad y
      especies distintas.
- [x] **B — Herencia roguelite.** El genoma sobrevive entre partidas y desbloqueás especies
      iniciales.
- [x] **C — Biomas.** Charco, estanque, musgo húmedo, intestino de termita (con protistas
      simbiontes reales), fuente hidrotermal y hielo marino.
- [ ] **D — Desafíos y eventos.** Desafío diario con seed compartida y eventos globales: bloom de
      algas, sequía, contaminación o invasión de fagos.
- [ ] **E — Editor de especies.** Diseñás una especie (organelos, comportamiento) y la soltás en un
      ecosistema. Se pueden compartir con otros jugadores.

**Favorita:** ___

**Notas / cambios / mejoras:**

>

---

## 15. Vida real vs. ficción

> Datos reales incómodos a decidir: **(1)** a esta escala no hay inercia (bajo Reynolds);
> **(2)** los tamaños varían enormemente: bacteria ~1 µm, paramecio ~200 µm, rotífero ~500 µm;
> **(3)** los tiempos son muy distintos: una bacteria se divide en ~20 min y un paramecio ~1 vez
> por día.

- [x] **A — Realismo estricto.** Solo especies y mecanismos reales. Las escalas de tiempo se
      comprimen pero respetan las proporciones.
- [ ] **B — Mecánica real, combinaciones libres.** Cada sistema funciona como en la naturaleza,
      pero el jugador puede combinar rasgos que nunca coexistieron.
- [ ] **C — Realismo legible.** Se exageran velocidades, colores y tamaños para que se entienda y
      sea divertido. Una enciclopedia in-game explica cómo es en la realidad.
- [ ] **D — Biología especulativa.** Vida microscópica de otro planeta, construida con principios
      reales (otra química, otra luz). Da libertad creativa total.
- [ ] **E — Modo dual.** El jugador elige al empezar entre "Modo ciencia" (realista) y "Modo
      arcade" (rápido y permisivo).

**Favorita:** ___

**Notas / cambios / mejoras:**

>
