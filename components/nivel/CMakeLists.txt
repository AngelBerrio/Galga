# Componente de lógica pura: no depende de drivers, por eso compila también para el target linux.
idf_component_register(SRCS "nivel.c"
                       INCLUDE_DIRS "include")
