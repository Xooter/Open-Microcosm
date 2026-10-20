# Plan: mecánicas nuevas de los seres (19/10/2026)

Seres de `candidatos.md` con mecánicas que el motor no tiene (prolongaciones, locomoción, defensas,
reacciones), de más fácil a más difícil, y cómo implementarlas sin código por especie: cada mecánica es
un **mecanismo con nombre** que los datos eligen (`AGENTS.md`). Las cifras marcadas «verificar» son
órdenes de magnitud: antes de cargarlas hay que leer la fuente.

Estado: ⬜ pendiente · 🟡 en curso · ✅ hecho.

## Mecanismos compartidos

| Id | Mecanismo | Datos | Implementación |
|---|---|---|---|
| M1 | Respuesta fóbica temporal a un campo (step-up/step-down) | `[[being.response]] field kind threshold action` | Generaliza `acidTrap`: la percepción muestrea el campo y el piloto guarda una memoria por respuesta |
| M2 | Mecanoestímulo hidrodinámico | `field = "shear"` | Deformación del agua junto a la célula (`Fluid`) + choque; reemplaza el `disturbed` ad hoc |
| M3 | Señal química liberada por un ser | `[[signal]]`, `[[being.release]]` | Un `ChemField` por señal, difunde solo si alguien la liberó; fuera del balance de P/N; escalar en el cultivo |
| M4 | Morfo inducido | `[[being.morph]]` | Componente `Morph`: transición gradual en tiempo biológico (`reshapeRestEven`) a otra silueta, dieta o boca |
| M5 | Apertura bucal | `mouth_width` [µm] | Ancho mínimo de Feret de la presa (cuerpo + prolongaciones) ≤ apertura; función pura de `bio/feeding`, también en el cultivo |
| M6 | Parálisis | componente `Paralyzed` | Sin locomoción, descarga ni huida; recuperación o muerte |
| M7 | Flagelo posterior (empujador) | `flagellum = "posterior"` | Invierte el dipolo del nadador y el lado de la propulsión |
| M8 | Cepas bacterianas como datos con taxis | `[[strain]] taxis` | Run & tumble con óptimo de concentración (bandas aerotácticas) |
| M9 | Tallo como varilla elástica | `[being.stalk]` | Cadena de nodos XPBD sin inercia, arrastrada por el agua, con contracción del espasmonema, crecimiento y tallo vacío |

## Lista (de más fácil a más difícil)

| # | Ser | Mecánica | Depende de | Estado |
|---|---|---|---|---|
| 1 | *Peridinium* | nado helicoidal | — | ✅ |
| 2 | *Peranema* | deslizamiento con el flagelo rígido | — | ✅ |
| 3 | *Pediastrum* | cuernos como prolongaciones del cuerpo | — | ✅ |
| 4 | *Halteria* | saltos espontáneos y de escape | M1, M2 | ✅ |
| 5 | *Blepharisma* (+ *Euglena*, *Chlamydomonas*) | fotofobia y daño fotodinámico | M1 | ✅ |
| 6 | *Colpoda* | quiste reproductivo (2–4 hijas) | autosporas, `Cyst` | ✅ |
| 7 | *Haematococcus* | pigmento por estrés que fotoprotege | 5 | ✅ |
| 8 | *Stentor* → *Spirostomum* | contracción del cuerpo entero | M2 | ✅ |
| 8b | sésiles | pie y tallo realistas | M2, 8 | ✅ |
| 9 | *Dileptus* (+ *Didinium*, *Litonotus*) | toxicistos y presa paralizada | M6 | ✅ |
| 10 | *Coleps* | histofagia: células dañadas, comida en grupo | M6 | ✅ |
| 11 | *Trachelomonas* | lórica que toma Fe del agua | balance | ✅ |
| 12 | *Dolichospermum* | acinetos | cianobacterias | ✅ |
| 13 | *Spirillum volutans* (+ *E. coli*) | aerotaxis en bandas | M8 | ✅ |
| 14 | depredadores | apertura bucal: los cuernos protegen | M5 | ✅ |
| 15 | *Lembadion* + *Euplotes* | kairomona y alas inducidas | M3, M4, M5 | ✅ |
| 16 | *Tetrahymena vorax* | microstoma → macrostoma | M3, M4, M5 | ✅ |
| 17 | *Ochromonas* (+ colonias de *Chlorella*) | mixotrofia; colonias inducidas | M3, M5 | 🟡 |
| 18 | *Actinophrys* | axopodios que atrapan al contacto | `Captured`, M6 | ✅ |
| 19 | *Vampyrella* | perfora la pared de *Spirogyra* | `Captured` | ✅ |
| 20 | *Salpingoeca* (+ espermatozoide) | collar filtrador, flagelo posterior | M7, M3 | 🟡 |
| 21 | *Lingulodinium* | bioluminiscencia por corte y reloj | M2, lente oscura | ✅ |
| 22 | *Mesodinium rubrum* | kleptoplastia y saltos | 4 | ✅ |
| 23 | *Paramecium bursaria* | *Chlorella* endosimbionte heredable | — | ✅ |
| 24 | *P. tetraurelia* killer | cuerpos R | M3 | ✅ |
| 25 | *Zoothamnium* / *Carchesium* | colonia ramificada con espasmonema | 8b | ✅ |
| 26 | *Bacillaria* | colonia que se desliza coordinada | glider | ✅ |
| 27 | tecamebas | testa rígida, seudópodos por la abertura | plasticidad por partícula | ✅ |
| 28 | *Dictyostelium* | relevo de AMPc | M3, división de amebas chicas | ✅ |

Fuera por ahora: reticulopodios (*Allogromia*), *Mixotricha*, parásitos intracelulares, crecimiento
apical, *Acetabularia*.

## Detalle por ser

1. **Peridinium.** El flagelo transversal (cíngulo) lo hace girar y el longitudinal lo empuja: hélice, que
   vista desde arriba es una oscilación lateral. `helix_radius`/`helix_period`: velocidad lateral
   sinusoidal sin inercia en `bio/locomotion`.
2. **Peranema.** Se desliza sobre el vidrio con el flagelo rígido adelante y solo la punta batiendo
   (Saito et al. 2003). `gliding = true`: avanza en su eje, sin bamboleo, poco arrastrado por el agua.
3. **Pediastrum.** Cuernos de las células marginales como `[[being.process]] inBody`.
4. **Halteria.** Saltos breves y rápidos con cirros saltadores, espontáneos y de escape ante la
   deformación del agua (Jakobsen 2001; Tamar 1979, verificar). M1/M2 con `action = "jump"`.
5. **Blepharisma.** Respuesta fóbica al aumento de luz (Kraml & Marwan 1983); la blefarismina es
   fotodinámica con luz intensa (Giese 1946): mantenimiento extra por luz alta. *Euglena* y
   *Chlamydomonas* con fotofobia (experimento 44).
6. **Colpoda.** Se enquista y se divide adentro en 2–4 (`division = "reproductive_cyst"`).
7. **Haematococcus.** Con luz alta y poco N acumula astaxantina en el quiste: baja el daño por luz y lo
   vira a rojo.
8. **Stentor / Spirostomum.** Mionemas (Ca²⁺): el cuerpo se acorta en ms y se re-extiende en segundos.
   Componente `Contraction` con forma contraída por `reshapeRestEven`. *Spirostomum* espera la división
   de las células largas.
8b. **Pie y tallo** (ver abajo).
9. **Dileptus.** Probóscide con toxicistos que paralizan (`Paralyzed`); la presa paralizada no descarga
   tricocistos.
10. **Coleps.** Las células dañadas (paralizadas, capturadas, en lisis) son presa aunque sean grandes;
    drenaje compartido.
11. **Trachelomonas.** La hija arma su lórica tomando Fe (`exchange`, `Flow::Uptake`); compartimento
    `Lorica` en el balance; el render la oscurece con el Fe.
12. **Dolichospermum.** Acinetos por estrés, como los heterocistos; sobreviven al filamento.
13. **Spirillum volutans.** Microaerófila: run & tumble con óptimo de O₂ (bandas).
14. **Apertura bucal.** `mouth_width` por depredador con fuente; reemplaza `smallPrey` y
    `maxPreyAreaRatio`.
15. **Lembadion/Euplotes.** Kairomona (Kuhlmann & Heckmann 1985); *Euplotes* con alas más anchas que
    la boca de *Lembadion*.
16. **T. vorax.** Estomatina de la presa (Smith-Somerville et al. 2000) → morfo macrostoma con otra dieta.
17. **Ochromonas.** Fotótrofa y fagótrofa (`mixotroph`); *Chlorella* forma colonias con la señal del
    depredador (Boraas et al. 1998).
18. **Actinophrys.** Axopodios de cubierta con alcance; captura (`Captured`) y copa fagocítica; flota.
19. **Vampyrella.** Se adhiere a una célula de *Spirogyra*, perfora y drena; queda la pared vacía.
20. **Salpingoeca.** Flagelo posterior (M7) y collar (`FeedingMode::Collar`); rosetas con señal
    bacteriana (Alegado et al. 2012).
21. **Lingulodinium.** Destello por esfuerzo de corte, capacidad con ritmo circadiano.
22. **Mesodinium.** Retiene los cloroplastos de *Cryptomonas* (`Kleptoplasts`), que decaen.
23. **P. bursaria.** `Endosymbionts`: azúcar con luz, reparto binomial en la mitosis.
24. **P. tetraurelia.** Cuerpos R ingeridos matan a los sensibles.
25. **Zoothamnium/Carchesium.** Árbol de tallos; espasmonema común o propio.
26. **Bacillaria.** Uniones deslizantes con fase común.
27. **Tecamebas.** Plasticidad por partícula: testa rígida, abertura plástica; granos minerales.
28. **Dictyostelium.** AMPc excitable con relevo y quimiotaxis al frente; decidir la escala de tiempo
    de la señal (difusión real contra cinética biológica ×60).

## 8b. Pie y tallo

**Hoy:** el tallo es una distancia (`applyAnchor`), siempre recto, contracción lineal de 4 s al 15 %,
igual para todos, aparece entero y desaparece con la célula; se dibuja como una línea de 1,2 px con
zigzag. *Stentor* tiene `stalk_length` pero en la realidad se fija con un pie.

**Real:**

| Tipo | Seres | Estructura | Comportamiento |
|---|---|---|---|
| Espasmonema | *Vorticella* | vaina elástica con el espasmonema en hélice descentrado; disco adhesivo | se dobla con el agua; se enrolla en hélice en ms (Ca²⁺; Amos 1971, Moriyama et al. 1998, Upadhyaya et al. 2008); se re-extiende en segundos; también contracciones espontáneas |
| Rígido | *Epistylis* | grueso, estriado | no se contrae (se contrae el zooide) |
| Rígido fino | suctorios | tubo refringente recto | casi no se dobla |
| Mucilaginoso | *Gomphonema* | polisacárido hialino ancho | crece siempre, flexible, se ramifica en la división |
| Pie | *Stentor* | polo posterior adherido | se suelta, nada y se vuelve a fijar |

Comunes: el tallo se secreta después de fijarse (horas biológicas); al irse o morir el zooide, el tallo
vacío queda y se degrada.

**Mecanismo (M9):** `bio/stalk` (sin raylib): nodos cada ~5 µm, restricciones de largo y flexión con
tasas [1/s], base fija, punta acoplada al polo posterior en las dos direcciones; cada nodo predice
`x + u_agua·dt` (se mece con las corrientes). CPU serial después de los cuerpos (determinista). La
contracción: disparo por M2 o espontáneo, largo de reposo a `contracted_fraction` y forma de hélice
proyectada, re-extensión con `extension_s`. Datos `[being.stalk]`: `kind`, `length`, `width`,
`bending_rate`, `growth_hours`, `decay_hours`, `contracted_fraction`, `extension_s`,
`spontaneous_per_min`; las claves viejas se siguen leyendo. Render: tubo con borde óptico, espasmonema
en hélice adentro, estrías, mucílago hialino, disco adhesivo, hélice de lazos al contraerse; una línea
de lejos. Primera etapa sin contactos célula–tallo.

## Orden de implementación (un commit por etapa)

1. Locomoción barata: 1, 2, 3.
2. M1 + M2: 4, 5, 7 (`acidTrap` migrado sin cambiar el resultado).
3. Contracción del cuerpo: 8 (*Stentor*).
3b. Pie y tallo: 8b.
4. M6: 9, 10.
5. M5 apertura bucal: 14.
6. M3 + M4: 15, 16, 17.
7. 6, 11, 12, 18, 19.
8. M7 + M8: 13, 20.
9. 21–28, cada uno con su plan.

## Avance (20/10/2026)

Hechas las etapas 1 a 9, un commit cada una (en la 9, uno por punto). Quedaron parciales:

- **17:** Ochromonas es mixótrofa, pero las colonias de Chlorella de Boraas et al. (1998) aparecen por
  selección en ~100 generaciones, no por una señal: esperan un gen de colonialidad.
- **20:** Salpingoeca tiene el flagelo posterior y el collar; las rosetas inducidas por Algoriphagus no
  (las bacterias no liberan señales) y el espermatozoide tampoco.
- **27:** la testa es el cuerpo blando rígido y los seudópodos son locomoción y dibujo (no plasticidad
  por partícula en el solver). Difflugia, con la testa de granos del sedimento, sigue pendiente.
- **M8:** Spirillum es una cepa más con masa propia; pasar las cepas a datos sigue pendiente.
- **8b:** los tallos todavía no chocan con las células.

Se arregló de paso la división de las amebas plásticas chicas (se destrabaron Acanthamoeba y
Naegleria): remallado fino y relajación polar durante la citocinesis.

**Tests:** escritos en `tests/test_mechanics.cpp` y `test_candidates.cpp`; a pedido del usuario se corren
al terminar el plan.

## Riesgos

- Tocar la percepción y el piloto cambia todas las corridas: un mecanismo por commit, streams de `Rng`
  nuevos.
- Contracción y plasticidad por partícula afectan `solve.comp` (GPU).
- Una FFT por señal: solo difunden si están activas.
- Escalas de tiempo de las señales (difusión real, cinética ×60).
- Paridad placa–cultivo: apertura bucal, mixotrofia y simbiontes en funciones sin cuerpo.
- Bloqueos: división de células largas y de amebas plásticas chicas; caso largo de *Ceratium*.
- Validación de Gause y *Didinium* con la apertura bucal.
- Compartimentos nuevos en `massBudget`.
