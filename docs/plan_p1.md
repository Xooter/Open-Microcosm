# Plan — Fase MVP P1: experimentos y aula

Plan del 16/10/2026, aprobado en sus decisiones. Se implementa en el orden de abajo, con un commit
por paso y sus tests. Avance: ✅ hecho · 🟡 en parte (se dice qué falta) · sin marca: pendiente.

## Decisiones tomadas

1. **Modo cultivo por individuos, sin espacio** (no ecuaciones de poblaciones). Cada célula conserva
   `Metabolism`, `Heredity` y `Lineage` y corre **la misma fisiología que la placa** (extraída de
   `World`, no copiada). Las bacterias son poblaciones por cepa (conteo entero con saltos de
   Poisson) con los mismos `BacteriaParams`.
2. **Encuentros por teoría física** (filtradores: aclaramiento de la corriente oral con el caudal
   máximo del citostoma; cazadores: núcleo de encuentro de Gerritsen y Strickler 1977 con las
   velocidades y radios de los datos) y **calibración cruzada placa ↔ cultivo**: un test mide la
   respuesta funcional en la placa completa y exige que el cultivo coincida.
3. **El cultivo se mira con una gota de muestra**: las células del volumen del campo se sortean
   (Poisson) y salen del cultivo; contar una alícuota da un conteo con error de muestreo. La ficha
   y la exportación declaran "modelo bien mezclado".
4. **Protocolo, no bloqueo opaco:** los parámetros del protocolo se ven con candado y motivo;
   destrabar es posible, queda como intervención y la exportación marca "fuera de protocolo". El
   bloqueo duro es opcional (exámenes). Aula y lecciones son un solo sistema: una lección es un
   protocolo con pasos.
5. **Lecciones sin puntaje:** predicción antes de correr (idea 36), preguntas con explicación,
   objetivo observable que verifica la simulación e informe exportable.
6. **Reactivos por mecanismo:** el reactivo trae un mecanismo con nombre y blancos químicos reales;
   la CI50 **emerge** de un lote de dosis con réplicas y se valida contra CIM publicadas (no es
   dato de entrada).
7. **Quimiostato solo en el cultivo;** en la placa, perfusión (renueva lo disuelto, las células
   quedan).
8. **Foto sin "aumento"** hasta que haya objetivos: barra de escala, ancho del campo, tiempo
   biológico, seed y tinción.
9. **Experimentos en TOML** (como los paquetes).
10. **Columnas del CSV con ids estables** (no cambian con el idioma); el rótulo traducido va al
    JSON.
11. **Programas antes que réplicas:** el protocolo de Gause necesita alimentación diaria.

## Pasos

0. ✅ **Una sola fisiología.** `bio/cell_physiology`: armado de la entrada de cada célula (µmax de los
   datos, rasgos, estrés de pH, temperatura y sal, quiste), crecimiento por bacteria compartido y
   cinética de las vacuolas separada de las partículas. Hecho cuando los checksums de `bio_bench`
   en IEEE estricto no cambian y pasa `ctest -L rapidos`. *Hecho:* checksums iguales en seis
   escenas (cuatro biomas, cultivo humano inoculado y epidermis de cebolla); `ingestContents` y
   `digestContents` para células sin cuerpo (`tests/test_cell_physiology.cpp`). El registro de
   parámetros sigue atado a `World`: se generaliza en el paso 1, cuando el cultivo lo necesite.
1. ✅ **Modelo de cultivo** (`game/culture`, en `biosim`): recipiente, medio bien mezclado, células,
   bacterias por cepa, detritos y fagos; luz por Beer–Lambert en la columna; el mismo balance de
   masa. Tests: conservación, determinismo 1/4 hilos, µmax de *Paramecium*.
   *Hecho:* células por individuo con la fisiología compartida, bacterias en cohortes, filtradores
   con respuesta funcional de tipo II (`ingestion_maxima` y `aclaramiento` de *P. caudatum* con
   fuente), cazadores con Gerritsen–Strickler, quistes, división con mutación, lisis, detritos y
   frústulas; el mismo balance de masa (cierra a 10⁻⁴); determinista por seed (es serial: no
   depende de los hilos). *Encontrado:* las células limitadas por la comida no se dividían nunca
   (invertían todo lo que pasaba del colchón de reservas y no llegaban al costo de la división);
   corregido en el metabolismo compartido con un punto de control al tamaño de división (también
   en la placa); la masa de las células alargadas salía de la esfera de su silueta (un paramecio
   pesaba casi el doble): ahora es la del esferoide (`espesor`); la placa usa la ingestión medida
   de la especie (antes, 4× sobrealimentada); los gases de la placa acompañan al Q10. Con eso el
   µmax de *Paramecium* pasa en el cultivo (0,023/h) y en la placa (0,024/h) contra 0,026/h. Luz
   atenuada por las fotótrofas en la columna (Beer–Lambert con la absorción de la clorofila) y fagos
   líticos con adsorción de acción de masas y mutantes resistentes. *Queda fuera (declarado en
   `Culture::supports` y en la cabecera de `game/culture`):* conjugación, Bdellovibrio, quorum,
   liberación de B₁₂ por las bacterias, colonias, filamentos, zoosporas y células animales.
2. ✅ **Calibración cruzada:** `bio_bench --respuesta-funcional`, test de validación placa ↔ cultivo.
   *Hecho:* las presas chicas tienen altura en la película (la placa tragaba todo lo superpuesto en
   2D), el aclaramiento con pocas presas sale de la escala de volúmenes corporales (el publicado es
   a alta densidad) y el radio de captura de bacterias se calibró con esa respuesta funcional; placa
   y cultivo coinciden dentro de un factor 2 entre 7·10⁴ y 8,5·10⁸ bacterias/mL.
3. ✅ **Programas** (`game/program`): rampas, pulsos y eventos periódicos, guardados como definición.
   *Hecho:* rampa, ciclo (fotoperíodo), seno y evento con la hora de laboratorio; acciones del
   cultivo (bacterias por mL, cambio de medio centrifugado, dilución, inocular); el cambio de medio
   y la dilución registran lo que sale (`Flow::Washout`) y lo que entra.
4. ✅ **Panel Cultivos y gota de muestra:** `.plate` v3 con seres explícitos con estado; conteo de
   alícuota con error de Poisson.
   *Hecho:* `game/culture_run` (sesión de cultivo con su serie, operaciones anotadas por paso y
   escena `.cultivo` que se repite igual), panel Cultivos (gradilla con velocidad propia, siembra,
   medio, programa, alícuota, gota de muestra concentrable, gráfico, guardar y exportar), la gota
   lleva las células con su estado a la placa (`WorldConfig::sampled`, `World::placeSampled`) y saca
   su volumen del cultivo. Medios `osterhout_gause` y `wc_guillard`; recipientes `tubo_gause`,
   `erlenmeyer_250` y `tubo_ensayo_10`.
5. 🟡 **Gause en el cultivo:** exclusión *P. aurelia* / *P. caudatum*; *Didinium*–*Paramecium* sin
   refugio y con inmigración.
   *Hecho:* con el protocolo de Gause (cambio de medio y ración diaria de bacterias, 26 °C) cada
   especie llega a una meseta, hay exclusión competitiva en el cultivo mixto y *Didinium* extingue a
   *Paramecium* y se queda sin activos. *Falta:* que gane *P. aurelia* como en Gause (en el modelo gana
   la que se adelanta: test `should_fail`, `docs/validacion.md`) y el caso con inmigración.
6. ✅ **Réplicas y lotes** (`game/experiment`, `bio_lote`, panel Experimento): media, DE, IC 95 % y n.
   *Hecho:* experimento en TOML (cultivo o placa, réplicas, factores con niveles), seeds por nivel y
   réplica, una corrida por hilo (el resultado no depende de los hilos), formato largo, resumen con
   media, DE e IC 95 % (t de Student) y JSON con procedencia y seeds; panel Experimento en segundo
   plano con banda de confianza.
7. ✅ **Quimiostato** (lavado binomial, `Flow::Washout`) y perfusión en la placa. Monod estacionario y
   exclusión completa de Tilman.
   *Hecho:* `Culture::setDilutionRate`; paso implícito para el sustrato de las bacterias (con el
   explícito el quimiostato quedaba en S ≈ 0); S* = 0,652 µM contra 0,647 de Monod; con D = 0,25/día,
   exclusión completa en los dos sentidos de Tilman. Perfusión de la placa
   (`medio.perfusion`, en tiempo biológico) con su entrada y salida en el balance.
8. ✅ **Siembra y dilución:** por densidad con Poisson, transferencias, diluciones seriadas.
   *Hecho:* inóculo por mL de cultivo (`inocular_densidad`) y desde una suspensión en la placa
   (Catálogo > Inocular, con los esperados); pasaje entre cultivos con las células con su estado
   (la escena del que recibe se repite); diluciones seriadas 1:10.
9. ✅ **Inglés:** `assets/lang/en.txt`, claves faltantes registradas, traducción del contenido por
   paquete con huella del texto de origen.
   *Hecho:* `assets/lang/en.txt` con 1330 textos (interfaz, parámetros, seres, medios, placas, biomas
   y minerales), selector en Preferencias, `idiomas/<código>.txt` por paquete (la clave es el texto en
   español: si cambia, la traducción deja de valer sola, sin huella aparte), `tools/claves_tr.py` y
   `BIO_TR_FALTANTES`, formatos traducibles verificados, columnas del CSV por id. *Falta:* los valores
   armados con números en la ficha (`game/inspect`).
10. ✅ **Protocolos y lecciones:** Jennings, Tilman, Gause, plasmólisis y fragilidad osmótica; cada
    una con su solución que un test recorre.
    *Hecho:* `game/lesson` (lecciones como datos en `lecciones.toml` y en los paquetes, condiciones
    con nombre que verifica la simulación, preguntas con explicación, predicciones contra lo medido,
    acciones con nombre), panel Lección (prepara la placa o el cultivo, muestra sus controles y
    acciones: el alumno no necesita otro panel) e informe en Markdown. Protocolo: los parámetros
    fijados se ven trabados con su motivo; destrabar se anota y la exportación dice
    `fuera_de_protocolo`. Parámetros de ambiente en el registro (`ambiente.*`). Operaciones del
    cultivo `quimiostato` y `medio_que_entra`; recipiente `microquimiostato`. Las cinco lecciones se
    completan con su solución (`tests/test_lessons.cpp`).
11. ✅ **Reactivos con mecanismos** y ajuste de Hill (`game/dose_fit`).
    *Hecho:* `reactivos.toml` (ampicilina y tetraciclina con la farmacodinámica medida de Regoes et
    al. 2004), muerte de las bacterias en el cultivo (por cohorte) y en la placa (un campo de reactivo
    que difunde solo si se usó, con el gotero), factor `reactivo:<id>` en los lotes con CI50 (Hill) y
    CIM en el JSON, medio Mueller-Hinton. La CIM emerge de la dilución en caldo: 4 contra 8 µg/mL
    (ampicilina) y 1 contra 1 (tetraciclina). *Falta:* metales y otros mecanismos (eucariotas).
12. ✅ **Programas en la placa.**
    *Hecho:* `WorldConfig::program` (en el `.plate` como `programa=`), aplicado por `World` una vez
    por paso con las horas biológicas (temperatura, luz, solutos, parámetros del registro, pulsos
    `agregar:<soluto>`, inóculos); intervención `Program` para cambiarlo en vivo; editor con presets en
    el panel Simulación.
13. **Foto y time-lapse** (por tiempo biológico, no por cuadro).
14. **Glosario y notas** (las notas van en la escena, no son intervenciones).
15. **Accesibilidad:** escala de interfaz, HiDPI y paleta Okabe–Ito.

## Riesgos

- Un refactor puede cambiar los últimos bits con `-ffast-math`: se compara en IEEE estricto.
- La calibración placa ↔ cultivo puede no cerrar: se decide con datos publicados, no ajustando.
- Gause puede no reproducirse con las temperaturas sin verificar: se informa tal cual.
- Réplicas en paralelo: precargar los registros, un hilo de OpenMP por réplica, motor CPU (la
  procedencia lo dice), tope de trabajadores por memoria.
- Ids en el CSV: rompe scripts que leían los nombres.

## Hecho cuando

Un experimento de Gause se corre en el modo cultivo con réplicas y se exporta con media y
dispersión, un quimiostato llega al estado estacionario de Monod, y un alumno sigue una lección en
inglés o en español sin tocar nada fuera de su panel.
