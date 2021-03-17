Consenso SKALE: un motor de consenso BFT en C ++
Discordia Construya y pruebe el consenso de skale

El consenso de SKALE utiliza múltiples proponentes de bloques. Los proponentes de bloques distribuyen propuestas a los nodos y recopilan pruebas de disponibilidad de datos basadas en firmas BLS. Luego, se ejecuta un acuerdo bizantino binario asincrónico para cada propuesta de bloque para llegar a un consenso sobre si hay datos disponibles. Si se sabe que existen múltiples propuestas de bloques con datos disponibles, se utiliza una moneda común basada en BLS para seleccionar la propuesta ganadora que está comprometida con la cadena.

SKALE Consensus utiliza un protocolo de acuerdo bizantino binario asincrónico (ABBA). La implementación actual utiliza ABBA de Mostefaoui et al. En general, se puede utilizar cualquier protocolo ABBA siempre que tenga las siguientes propiedades:

Modelo de red: el protocolo asume un modelo de mensajería de red asíncrona.
Nodos bizantinos: el protocolo asume que menos de 1/3 de los nodos son bizantinos.
Voto inicial: el protocolo asume que cada nodo hace un voto inicial de sí o no .
Voto de consenso: el protocolo finaliza con un voto de consenso de sí o no . Cuando el voto por consenso es sí , se garantiza que al menos un nodo honesto votó sí .
Una nota importante sobre la preparación para la producción:
El consenso SKALE todavía está en desarrollo activo y contiene errores. Este software debe considerarse software alfa . El desarrollo aún está sujeto a la competencia de las especificaciones, el refuerzo de la seguridad, las pruebas adicionales y los cambios importantes. Este motor de consenso aún no ha sido revisado ni auditado por seguridad. Consulte SECURITY.md para conocer las políticas de informes.

Mapa vial
para ser publicado pronto

requerimientos de instalación
El consenso de SKALE se ha construido y probado en Ubuntu.

Asegúrese de que los paquetes necesarios estén instalados ejecutando:

sudo apt-get update
sudo apt-get install -yq libprocps-dev g ++ - 7 valgrind gawk sed libffi-dev ccache \
    libgoogle-perftools-dev flex bison yasm texinfo autotools-dev automake \
    python python-pip cmake libtool build-essential pkg-config autoconf wget \
    git libargtable2-dev libmicrohttpd-dev libhiredis-dev redis-server openssl \
    libssl-dev doxygen
Construyendo desde código fuente en Ubuntu (Desarrollo)
Clonar proyecto y configurar compilación:

git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
 # Configure el proyecto y cree un directorio de compilación. 
guiones de cd ; ./build_deps.sh # construir dependencias 
cd .. ; cmake . -Bbuild # Configura la construcción. 
cmake --build build - -j $ ( nproc )  # Construye todos los objetivos predeterminados usando todos los núcleos.
Ejecutando pruebas
Navegue a los directorios de prueba y ejecute ./consensusd .

Bibliotecas
libBLS por SKALE Labs
Contribuyendo
Si tiene alguna pregunta, consulte a nuestra comunidad de desarrollo en Discord .

Discordia

Licencia
Licencia

Copyright (C) 2018-presente SKALE Labs
