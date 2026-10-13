# BioGame — 50 ideas para el sandbox

Ideas para un sandbox educativo, científico y de código abierto. Marcá con `[x]` las que te gusten.

## Experimentos y control del laboratorio

- [ ] 1. **Escenas guardables:** exportar e importar una gota (bioma, seed, parámetros del medio y
      seres puestos a mano) como archivo de texto, para compartirla y reproducirla.
- [ ] 2. **Línea de tiempo con replays:** grabar la sesión por seed y eventos, y rebobinar o
      avanzar a cualquier instante (la simulación ya es determinista).
- [ ] 3. **Experimentos por lote:** correr N gotas con un parámetro barrido (por ejemplo, Si de 0
      a 300 µM) sin ventana y exportar los resultados.
- [ ] 4. **Rampas y programas:** variar la temperatura, la luz o los nutrientes con el tiempo
      (ciclo día/noche, pulso de fosfato, estación).
- [ ] 5. **Controles y réplicas:** dos gotas lado a lado con la misma seed, cambiando una sola
      variable, para comparar.
- [ ] 6. **Antibióticos, sal, ácidos y toxinas** como reactivos que se agregan con el gotero y
      tienen dosis-respuesta por especie.
- [ ] 7. **Dilución y siembra:** pasar una alícuota a una gota nueva (cultivos seriados), como en
      un laboratorio de microbiología.
- [ ] 8. **Quimiostato:** flujo continuo de medio nuevo y salida de agua, para estudiar
      crecimiento en estado estacionario (Monod).
- [ ] 9. **Pinzas ópticas:** sujetar y arrastrar una célula con el mouse sin dañarla, con la fuerza
      real en pN.
- [ ] 10. **Micropipeta:** aspirar una célula o un grupo de bacterias de un punto y soltarlas en otro.

## Medición y análisis

- [ ] 11. **Gráficos en vivo:** curvas de población por especie, nutrientes, O₂ y pH, con zoom y
      exportación a CSV.
- [ ] 12. **Balance de masa visible:** diagrama de flujos de P, N, Si, Fe y C entre agua, células,
      detritos y sedimento, que cierre al 100 %.
- [ ] 13. **Regla y escala:** medir distancias y áreas en µm directamente sobre la imagen.
- [ ] 14. **Seguimiento de trayectorias:** dibujar la estela de una célula y calcular su velocidad
      media, el desplazamiento cuadrático medio y el tipo de movimiento.
- [ ] 15. **Cuantificación por región:** contar células y medir concentraciones dentro de un área
      que se dibuja a mano.
- [ ] 16. **Curva de crecimiento automática:** tiempo de duplicación, fase de latencia y capacidad
      de carga ajustadas a los datos de la gota.
- [ ] 17. **Mapa de limitación:** colorear cada célula según el nutriente que limita su
      crecimiento en ese momento (P, N, Si, Fe, B₁₂, luz).
- [ ] 18. **Panel de cuotas celulares:** ver cuántas veces la cuota mínima tiene cada nutriente
      dentro de una célula elegida.
- [ ] 19. **Árbol de linaje:** reconstruir la genealogía de una población, con mutaciones por
      rama y tasa de selección.
- [ ] 20. **Exportar datos abiertos:** CSV/JSON de series temporales y de la ficha de cada especie,
      con las unidades y los parámetros usados.

## Microscopía y óptica

- [ ] 21. **Lentes intercambiables:** campo claro, contraste de fase, campo oscuro y
      fluorescencia, cada uno con la información que muestra el real.
- [ ] 22. **Tinciones reales:** Gram, lugol (almidón), DAPI (núcleos), rojo neutro, azul de
      metileno y tinta china para cápsulas.
- [ ] 23. **Fluorescencia de clorofila:** las algas brillan en rojo bajo luz azul, y el brillo
      baja cuando les falta hierro.
- [ ] 24. **Objetivos de 10×, 40× y 100× con aceite:** el aumento cambia el campo y la
      profundidad, y a 100× se ven las bacterias individuales.
- [ ] 25. **Foto de microscopio con barra de escala**, fecha, aumento y seed, lista para
      publicar o usar en clase.
- [ ] 26. **Video con intervalo (time-lapse):** acelerar horas biológicas en un clip de segundos.
- [ ] 27. **Campo oscuro de Tyndall:** las partículas diminutas (arcilla, bacterias) dispersan la
      luz y se ven brillantes.
- [ ] 28. **Polarización:** los cristales (frústulas, cuarzo, almidón) cambian de color al
      girar el polarizador.
- [ ] 29. **Cámara cenital con lupa:** un recuadro flotante de mayor aumento sobre lo que está
      bajo el cursor.
- [ ] 30. **Modo fantasma de datos:** superponer la concentración de un nutriente solo dentro de
      las células como un halo.

## Contenido educativo

- [ ] 31. **Lecciones guiadas:** recorridos cortos con un objetivo observable ("¿por qué no se
      divide la diatomea sin sílice?") y un cuestionario.
- [ ] 32. **Modo aula:** el docente prepara una gota con seed fija y los alumnos la comparan, sin
      depender del azar.
- [ ] 33. **Notas en la ficha:** el usuario escribe observaciones ancladas a un ser, un lugar o
      un instante, y se exportan junto al experimento.
- [ ] 34. **Fichas con referencias:** cada ficha cita la fuente científica de sus parámetros
      (tasas, tamaños, cuotas) y marca qué se simplificó.
- [ ] 35. **Glosario interactivo:** términos como Reynolds, quimiotaxis o Redfield con una
      animación mínima dentro del programa.
- [ ] 36. **Hipótesis y predicción:** antes de correr, el usuario predice el resultado y el
      programa lo compara con lo que pasó.
- [ ] 37. **Escenarios históricos:** Gran Oxidación, Tierra bola de nieve, eutrofización de un
      lago y floración de cianobacterias.
- [ ] 38. **Casos reales:** reproducir experimentos clásicos (Gause con *Paramecium*, Tilman con
      diatomeas y sílice, Jennings con CO₂).
- [ ] 39. **Traducción de la interfaz:** textos en un archivo aparte para sumar idiomas y acentos.
- [ ] 40. **Modo accesible:** paleta para daltonismo, texto grande y subtítulos de los eventos.

## Biología y química nuevas

- [ ] 41. **Ciclo del silicio completo:** disolución dependiente del pH y la temperatura, y
      sedimentación de frústulas que sale del campo de visión.
- [ ] 42. **Quelación del hierro:** sideróforos de bacterias y algas que compiten por el Fe, con
      precipitación a óxido a pH alto.
- [ ] 43. **Más vitaminas y micronutrientes:** tiamina (B₁), biotina (B₇), zinc, cobalto, molibdeno
      y manganeso, con auxotrofías reales por especie.
- [ ] 44. **Carbono inorgánico:** CO₂ limitante, mecanismos concentradores de carbono y su efecto
      en el pH.
- [ ] 45. **Endosimbiosis y kleptoplastia:** tragar un alga y conservarla, con la restricción de
      cada caso real, como experimento y no como mejora.
- [ ] 46. **Biofilms y matriz extracelular:** tapetes bacterianos que cambian el flujo y retienen
      nutrientes.
- [ ] 47. **Epidemias y resistencia:** fagos, antibióticos y evolución de la resistencia con
      costo, observable en el árbol de linaje.
- [ ] 48. **Suelo y agua de verdad:** cargar una muestra con parámetros medidos (T, pH,
      conductividad, nutrientes) y ver qué comunidad sostiene.

## Técnica y comunidad

- [ ] 49. **Especies como datos:** archivos TOML por especie, para que la comunidad sume organismos
      sin tocar C++ (con validación de unidades y de coherencia biológica).
- [ ] 50. **Núcleo sin ventana y bindings:** exponer `biosim` como librería con una interfaz
      mínima (C o Python) para usar la simulación en investigación y cuadernos de clase.
