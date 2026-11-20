// ============================================================
// LEXER DE UrbanSimDSL (sintaxis en espanol) - version C++
// ------------------------------------------------------------
// Adecuado a los 30 ejercicios del documento "Alcance y Sintaxis
// del Lenguaje". Mantiene la arquitectura de la plantilla
// graficador.php: TablaLexica + Token + Tokenizador, con los
// mismos nombres de metodo y el algoritmo de "lexema mas largo"
// (Maximal Munch) para los simbolos fijos.
//
// + ANALIZADOR SINTACTICO (PDA de urbansim_dpda_estricto.jff)
// + ANALIZADOR SEMANTICO (catalogo de errores semanticos)
//
// Compilar:  g++ -std=c++17 -Wall -Wextra -o urbansim_semantico urbansim_semantico.cpp
// ============================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>

// ------------------------------------------------------------
// TOKENS
// Se renumeran porque el conjunto de palabras reservadas cambio
// (ahora son las del lenguaje en espanol). Se conserva el sufijo
// _KW en las palabras propias de programacion (if, for, ...).
// ------------------------------------------------------------
enum class Tok : int
{
    // Literales
    ID = 0,
    NUM = 1,
    CADENA = 2,

    // Delimitadores
    APAR = 3,
    CPAR = 4,
    ALLAVE = 5,
    CLLAVE = 6,
    ACORCHETE = 7,
    CCORCHETE = 8,

    // Separadores / acceso
    COMA = 9,
    PUNTO = 10,

    // Asignacion
    IGUAL = 11,

    // Aritmeticos
    MAS = 12,
    MENOS = 13,
    POR = 14,
    DIV = 15,

    // Relacionales
    IGUALIGUAL = 16,
    DIFERENTE = 17,
    MENORQUE = 18,
    MAYORQUE = 19,
    MENORIGUAL = 20,
    MAYORIGUAL = 21,

    // Logicos
    Y = 22,
    O = 23,
    NO = 24,

    // Porcentaje
    PORCIENTO = 25,

    // Escenario
    ESCENARIO = 26,
    MAPA = 27,

    // Vias
    VIA = 28,
    TIPO = 29,
    PUNTOS = 30,
    CARRILES = 31,
    ANCHO_CARRIL = 32,
    SENTIDO = 33,
    VELOCIDAD_MAX = 34,
    CAPACIDAD = 35,

    // Intersecciones
    INTERSECCION = 36,
    POSICION = 37,
    CONECTA = 38,

    // Semaforos
    SEMAFORO = 39,
    CICLO = 40,
    FASE = 41,
    DURACION = 42,

    // Edificios
    EDIFICIO = 43,
    DIMENSIONES = 44,
    LARGO = 45,
    ANCHO = 46,
    ALTURA = 47,
    ROTACION = 48,

    // Puentes
    PUENTE = 49,
    ORIENTACION = 50,
    GEOMETRIA = 51,
    LONGITUD = 52,
    CALZADA = 53,
    SOBRE = 54,
    ESTRUCTURA = 55,
    TABLERO = 56,
    APOYO = 57,

    // Demanda vehicular
    DEMANDA = 58,
    FLUJO = 59,
    INICIO = 60,
    COMPOSICION = 61,

    // Clima y eventos
    CLIMA = 62,
    ESTADO = 63,
    INTENSIDAD = 64,
    VISIBILIDAD = 65,
    EVENTO = 66,
    CARRIL = 67,

    // Simulacion
    SIMULACION = 68,
    INTERVALO = 69,
    REPETICIONES = 70,

    // Construcciones avanzadas del lenguaje
    FUNCTION_KW = 71,
    VAR_KW = 72,
    OBJECT_KW = 73,
    IF_KW = 74,
    ELSE_KW = 75,
    FOR_KW = 76,
    IN_KW = 77,
    RETURN_KW = 78,
    TRUE_KW = 79,
    FALSE_KW = 80,

    // Valores enumerados (tipo de via, sentido, colores, clima, vehiculos)
    AVENIDA = 81,
    CALLE = 82,
    DOBLE = 83,
    UNICO = 84,
    VEHICULAR = 85,
    VERDE = 86,
    AMARILLO = 87,
    ROJO = 88,
    LLUVIA = 89,
    DESPEJADO = 90,
    BAJA = 91,
    MEDIA = 92,
    ALTA = 93,
    ACCIDENTE = 94,
    AUTO = 95,
    AUTOS = 96,
    BUS = 97,
    BUSES = 98,
    MOTOS = 99,
    TAXI = 100,
    CAMIONES = 101,

    // Unidades de medida
    UNIDAD_M = 102,
    UNIDAD_KM = 103,
    UNIDAD_KMH = 104,
    UNIDAD_S = 105,
    UNIDAD_MIN = 106,
    UNIDAD_H = 107,
    UNIDAD_VEHH = 108,
    UNIDAD_GRADOS = 109,

    FIN = 666,
    ERROR = 999
};

// Helpers: ctype exige unsigned char para evitar comportamiento
// indefinido con caracteres negativos (bytes UTF-8 > 127).
static bool EsLetra(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
static bool EsDigito(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
static bool EsEspacio(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

/* ============================================================
 * CLASE TABLA LEXICA
 * ============================================================ */

// Equivale al array ['token'=>..., 'lexema'=>..., 'tipo'=>...]
struct Entrada
{
    Tok token;
    std::string lexema;
    std::string tipo;
};

// Equivale al array devuelto por BuscarLexemaMasLargo (+ 'longitud')
struct Coincidencia
{
    Tok token;
    std::string lexema;
    std::string tipo;
    std::size_t longitud;
};

class TablaLexica
{
private:
    std::map<Tok, Entrada> porToken;
    std::map<std::string, Entrada> porLexema;

    void Agregar(Tok token, const std::string &lexema, const std::string &tipo)
    {
        Entrada dato{token, lexema, tipo};
        porToken[token] = dato;   // indice por TOKEN
        porLexema[lexema] = dato; // indice por LEXEMA
    }

public:
    TablaLexica()
    {
        // Escenario
        Agregar(Tok::ESCENARIO, "escenario", "palabra_reservada");
        Agregar(Tok::MAPA, "mapa", "palabra_reservada");

        // Vias
        Agregar(Tok::VIA, "via", "palabra_reservada");
        Agregar(Tok::TIPO, "tipo", "palabra_reservada");
        Agregar(Tok::PUNTOS, "puntos", "palabra_reservada");
        Agregar(Tok::CARRILES, "carriles", "palabra_reservada");
        Agregar(Tok::ANCHO_CARRIL, "ancho_carril", "palabra_reservada");
        Agregar(Tok::SENTIDO, "sentido", "palabra_reservada");
        Agregar(Tok::VELOCIDAD_MAX, "velocidad_max", "palabra_reservada");
        Agregar(Tok::CAPACIDAD, "capacidad", "palabra_reservada");

        // Intersecciones
        Agregar(Tok::INTERSECCION, "interseccion", "palabra_reservada");
        Agregar(Tok::POSICION, "posicion", "palabra_reservada");
        Agregar(Tok::CONECTA, "conecta", "palabra_reservada");

        // Semaforos
        Agregar(Tok::SEMAFORO, "semaforo", "palabra_reservada");
        Agregar(Tok::CICLO, "ciclo", "palabra_reservada");
        Agregar(Tok::FASE, "fase", "palabra_reservada");
        Agregar(Tok::DURACION, "duracion", "palabra_reservada");

        // Edificios
        Agregar(Tok::EDIFICIO, "edificio", "palabra_reservada");
        Agregar(Tok::DIMENSIONES, "dimensiones", "palabra_reservada");
        Agregar(Tok::LARGO, "largo", "palabra_reservada");
        Agregar(Tok::ANCHO, "ancho", "palabra_reservada");
        Agregar(Tok::ALTURA, "altura", "palabra_reservada");
        Agregar(Tok::ROTACION, "rotacion", "palabra_reservada");

        // Puentes
        Agregar(Tok::PUENTE, "puente", "palabra_reservada");
        Agregar(Tok::ORIENTACION, "orientacion", "palabra_reservada");
        Agregar(Tok::GEOMETRIA, "geometria", "palabra_reservada");
        Agregar(Tok::LONGITUD, "longitud", "palabra_reservada");
        Agregar(Tok::CALZADA, "calzada", "palabra_reservada");
        Agregar(Tok::SOBRE, "sobre", "palabra_reservada");
        Agregar(Tok::ESTRUCTURA, "estructura", "palabra_reservada");
        Agregar(Tok::TABLERO, "tablero", "palabra_reservada");
        Agregar(Tok::APOYO, "apoyo", "palabra_reservada");

        // Demanda vehicular
        Agregar(Tok::DEMANDA, "demanda", "palabra_reservada");
        Agregar(Tok::FLUJO, "flujo", "palabra_reservada");
        Agregar(Tok::INICIO, "inicio", "palabra_reservada");
        Agregar(Tok::COMPOSICION, "composicion", "palabra_reservada");

        // Clima y eventos
        Agregar(Tok::CLIMA, "clima", "palabra_reservada");
        Agregar(Tok::ESTADO, "estado", "palabra_reservada");
        Agregar(Tok::INTENSIDAD, "intensidad", "palabra_reservada");
        Agregar(Tok::VISIBILIDAD, "visibilidad", "palabra_reservada");
        Agregar(Tok::EVENTO, "evento", "palabra_reservada");
        Agregar(Tok::CARRIL, "carril", "palabra_reservada");

        // Simulacion
        Agregar(Tok::SIMULACION, "simulacion", "palabra_reservada");
        Agregar(Tok::INTERVALO, "intervalo", "palabra_reservada");
        Agregar(Tok::REPETICIONES, "repeticiones", "palabra_reservada");

        // Construcciones avanzadas (funciones, variables, objetos, control)
        Agregar(Tok::FUNCTION_KW, "function", "palabra_reservada");
        Agregar(Tok::VAR_KW, "var", "palabra_reservada");
        Agregar(Tok::OBJECT_KW, "object", "palabra_reservada");
        Agregar(Tok::IF_KW, "if", "palabra_reservada");
        Agregar(Tok::ELSE_KW, "else", "palabra_reservada");
        Agregar(Tok::FOR_KW, "for", "palabra_reservada");
        Agregar(Tok::IN_KW, "in", "palabra_reservada");
        Agregar(Tok::RETURN_KW, "return", "palabra_reservada");
        Agregar(Tok::TRUE_KW, "true", "palabra_reservada");
        Agregar(Tok::FALSE_KW, "false", "palabra_reservada");

        // Valores enumerados del dominio
        Agregar(Tok::AVENIDA, "avenida", "valor");
        Agregar(Tok::CALLE, "calle", "valor");
        Agregar(Tok::DOBLE, "doble", "valor");
        Agregar(Tok::UNICO, "unico", "valor");
        Agregar(Tok::VEHICULAR, "vehicular", "valor");
        Agregar(Tok::VERDE, "verde", "valor");
        Agregar(Tok::AMARILLO, "amarillo", "valor");
        Agregar(Tok::ROJO, "rojo", "valor");
        Agregar(Tok::LLUVIA, "lluvia", "valor");
        Agregar(Tok::DESPEJADO, "despejado", "valor");
        Agregar(Tok::BAJA, "baja", "valor");
        Agregar(Tok::MEDIA, "media", "valor");
        Agregar(Tok::ALTA, "alta", "valor");
        Agregar(Tok::ACCIDENTE, "accidente", "valor");
        Agregar(Tok::AUTO, "auto", "valor");
        Agregar(Tok::AUTOS, "autos", "valor");
        Agregar(Tok::BUS, "bus", "valor");
        Agregar(Tok::BUSES, "buses", "valor");
        Agregar(Tok::MOTOS, "motos", "valor");
        Agregar(Tok::TAXI, "taxi", "valor");
        Agregar(Tok::CAMIONES, "camiones", "valor");

        // Unidades de medida (se reconocen como identificadores y
        // luego se buscan en la tabla, igual que las reservadas)
        Agregar(Tok::UNIDAD_M, "m", "unidad");
        Agregar(Tok::UNIDAD_KM, "km", "unidad");
        Agregar(Tok::UNIDAD_KMH, "kmh", "unidad");
        Agregar(Tok::UNIDAD_S, "s", "unidad");
        Agregar(Tok::UNIDAD_MIN, "min", "unidad");
        Agregar(Tok::UNIDAD_H, "h", "unidad");
        Agregar(Tok::UNIDAD_VEHH, "veh_h", "unidad");
        Agregar(Tok::UNIDAD_GRADOS, "grados", "unidad");

        // Delimitadores
        Agregar(Tok::APAR, "(", "simbolo");
        Agregar(Tok::CPAR, ")", "simbolo");
        Agregar(Tok::ALLAVE, "{", "simbolo");
        Agregar(Tok::CLLAVE, "}", "simbolo");
        Agregar(Tok::ACORCHETE, "[", "simbolo");
        Agregar(Tok::CCORCHETE, "]", "simbolo");

        // Separadores y acceso a propiedades (objeto.propiedad)
        Agregar(Tok::COMA, ",", "separador");
        Agregar(Tok::PUNTO, ".", "separador");

        // Asignacion
        Agregar(Tok::IGUAL, "=", "operador");

        // Aritmeticos
        Agregar(Tok::MAS, "+", "operador");
        Agregar(Tok::MENOS, "-", "operador");
        Agregar(Tok::POR, "*", "operador");
        Agregar(Tok::DIV, "/", "operador");

        // Relacionales ("==" gana sobre "=" por lexema mas largo)
        Agregar(Tok::IGUALIGUAL, "==", "operador");
        Agregar(Tok::DIFERENTE, "!=", "operador");
        Agregar(Tok::MENORQUE, "<", "operador");
        Agregar(Tok::MAYORQUE, ">", "operador");
        Agregar(Tok::MENORIGUAL, "<=", "operador");
        Agregar(Tok::MAYORIGUAL, ">=", "operador");

        // Logicos
        Agregar(Tok::Y, "&&", "operador");
        Agregar(Tok::O, "||", "operador");
        Agregar(Tok::NO, "!", "operador");

        // Porcentaje (composicion de demanda, visibilidad)
        Agregar(Tok::PORCIENTO, "%", "simbolo");
    }

    std::optional<Entrada> BuscarPorToken(Tok token) const
    {
        auto it = porToken.find(token);
        if (it == porToken.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<Entrada> BuscarPorLexema(const std::string &lexema) const
    {
        auto it = porLexema.find(lexema);
        if (it == porLexema.end())
            return std::nullopt;
        return it->second;
    }

    bool ExisteLexema(const std::string &lexema) const
    {
        return porLexema.count(lexema) > 0;
    }

    // Busca el lexema fijo valido MAS LARGO desde 'posicion'
    // (= == ; < <= ; ! != ; etc.)
    std::optional<Coincidencia> BuscarLexemaMasLargo(const std::string &codigo,
                                                     std::size_t posicion) const
    {
        std::optional<Coincidencia> encontrado;

        for (const auto &[lexema, datos] : porLexema)
        {
            // Palabras reservadas, valores y unidades (empiezan con letra)
            // se reconocen antes en el Tokenizador.
            if (EsLetra(lexema[0]))
                continue;

            std::size_t longitud = lexema.size();

            // equivalente a substr($codigo,$posicion,$longitud) === $lexema
            if (codigo.compare(posicion, longitud, lexema) == 0)
            {
                if (!encontrado || longitud > encontrado->longitud)
                {
                    encontrado = Coincidencia{datos.token, lexema, datos.tipo, longitud};
                }
            }
        }

        return encontrado;
    }

    const std::map<Tok, Entrada> &getTabla() const { return porToken; }
};

/* ============================================================
 * CLASE TOKEN
 * ============================================================ */

class Token
{
public:
    Tok token;
    std::optional<std::string> lexema; // ?string en PHP
    int linea;
    int columna;

    Token(Tok token, std::optional<std::string> lexema, int linea, int columna)
        : token(token), lexema(std::move(lexema)), linea(linea), columna(columna) {}

    void Mostrar() const
    {
        std::cout << "Token(" << static_cast<int>(token) << ") \t";
        std::cout << "Lexema(" << (lexema ? *lexema : "null") << ") \t";
        std::cout << "Linea(" << linea << ") \t";
        std::cout << "Columna(" << columna << ")";
        std::cout << std::endl;
    }
};

/* ============================================================
 * CLASE TOKENIZADOR
 * ============================================================ */

class Tokenizador
{
private:
    std::string codigo;
    std::size_t indice = 0;
    int linea = 1;
    int columna = 1;
    const TablaLexica &tablaLexica;

    bool FinCodigo() const { return indice >= codigo.size(); }

    // Mira un caracter adelante SIN consumirlo (nullopt = fuera de rango)
    std::optional<char> Ver(std::size_t adelanto = 0) const
    {
        std::size_t posicion = indice + adelanto;
        if (posicion >= codigo.size())
            return std::nullopt;
        return codigo[posicion];
    }

    void Avanzar()
    {
        if (FinCodigo())
            return;

        if (codigo[indice] == '\n')
        {
            linea++;
            columna = 1;
        }
        else
        {
            columna++;
        }

        indice++;
    }

    // Omite espacios y comentarios // y /* */
    void OmitirEspacios()
    {
        while (!FinCodigo())
        {
            char caracter = codigo[indice];

            if (EsEspacio(caracter))
            {
                Avanzar();
                continue;
            }

            // Comentario de linea
            if (caracter == '/' && Ver(1) == '/')
            {
                while (!FinCodigo() && codigo[indice] != '\n')
                    Avanzar();
                continue;
            }

            // Comentario de bloque
            if (caracter == '/' && Ver(1) == '*')
            {
                Avanzar(); // '/'
                Avanzar(); // '*'

                while (!FinCodigo() &&
                       !(codigo[indice] == '*' && Ver(1) == '/'))
                {
                    Avanzar();
                }

                if (!FinCodigo())
                {
                    Avanzar(); // '*'
                    Avanzar(); // '/'
                }
                continue;
            }

            break;
        }
    }

    // letra o '_' al inicio; luego letras, digitos o '_'
    std::string LeerIdentificador()
    {
        std::string lexema;

        while (!FinCodigo())
        {
            char caracter = codigo[indice];

            if (!EsLetra(caracter) && !EsDigito(caracter) && caracter != '_')
                break;

            lexema += caracter;
            Avanzar();
        }

        return lexema;
    }

    // Permite 10, 3.50, 0.30. No permite 12.4.5 (devuelve nullopt)
    std::optional<std::string> LeerNumero()
    {
        std::string lexema;
        int cantidadPuntos = 0;

        while (!FinCodigo())
        {
            char caracter = codigo[indice];

            if (EsDigito(caracter))
            {
                lexema += caracter;
                Avanzar();
                continue;
            }

            if (caracter == '.')
            {
                cantidadPuntos++;

                if (cantidadPuntos > 1)
                {
                    // consumir el caracter del error para no quedar
                    // detenidos en la misma posicion
                    Avanzar();
                    return std::nullopt;
                }

                lexema += caracter;
                Avanzar();
                continue;
            }

            break;
        }

        return lexema;
    }

    // "Av_Floral" -> Av_Floral (nullopt si falta la comilla final)
    std::optional<std::string> LeerCadena()
    {
        Avanzar(); // comilla doble inicial

        std::string lexema;

        while (!FinCodigo())
        {
            char caracter = codigo[indice];

            if (caracter == '"')
            {
                Avanzar();
                return lexema;
            }

            lexema += caracter;
            Avanzar();
        }

        return std::nullopt;
    }

    std::optional<Coincidencia> LeerLexemaFijo()
    {
        auto elemento = tablaLexica.BuscarLexemaMasLargo(codigo, indice);

        if (!elemento)
            return std::nullopt;

        for (std::size_t i = 0; i < elemento->longitud; i++)
            Avanzar();

        return elemento;
    }

public:
    Tokenizador(std::string codigo, const TablaLexica &tablaLexica)
        : codigo(std::move(codigo)), tablaLexica(tablaLexica) {}

    Token getToken()
    {
        OmitirEspacios();

        if (FinCodigo())
            return Token(Tok::FIN, std::nullopt, linea, columna);

        int lineaInicial = linea;
        int columnaInicial = columna;

        char caracter = codigo[indice];

        // IDENTIFICADOR, PALABRA RESERVADA, VALOR O UNIDAD
        if (EsLetra(caracter) || caracter == '_')
        {
            std::string lexema = LeerIdentificador();

            if (auto elemento = tablaLexica.BuscarPorLexema(lexema))
                return Token(elemento->token, lexema, lineaInicial, columnaInicial);

            return Token(Tok::ID, lexema, lineaInicial, columnaInicial);
        }

        // NUMERO
        if (EsDigito(caracter))
        {
            auto lexema = LeerNumero();

            if (!lexema)
                return Token(Tok::ERROR, std::nullopt, lineaInicial, columnaInicial);

            return Token(Tok::NUM, lexema, lineaInicial, columnaInicial);
        }

        // CADENA (comilla doble)
        if (caracter == '"')
        {
            auto lexema = LeerCadena();

            if (!lexema)
                return Token(Tok::ERROR, std::nullopt, lineaInicial, columnaInicial);

            return Token(Tok::CADENA, lexema, lineaInicial, columnaInicial);
        }

        // LEXEMA FIJO
        if (auto elemento = LeerLexemaFijo())
            return Token(elemento->token, elemento->lexema, lineaInicial, columnaInicial);

        // CARACTER DESCONOCIDO
        Avanzar();
        return Token(Tok::ERROR, std::string(1, caracter), lineaInicial, columnaInicial);
    }
};

/* ============================================================
 * ANALIZADOR SINTACTICO (automata de pila)
 * ------------------------------------------------------------
 * Implementa el PDA de "urbansim_dpda_estricto.jff".
 *
 * Igual que el AnalizadorSintactico del PHP del ingeniero, pero
 * en lugar de una tabla [estado][token] => estado, la tabla es
 * [estado][simbolo][tope de pila] => estado + operacion de pila.
 *
 * Alfabeto de pila:
 *   Z = fondo      E = escenario    B = bloque { }
 *   H = sub-bloque { } (fase, composicion, estructura, ...)
 *   K = lista [ ]  P = tupla ( )
 *
 * Los tokens del lexer se agrupan en las clases que usa el
 * automata (PROP, VALOR, UNIDAD) mediante Clasificar().
 * ============================================================ */

// Simbolos de entrada del automata (clases de token)
enum class Sim
{
    VAR, ESCENARIO, MAPA, VIA, PUENTE, EVENTO, SEMAFORO, INTERSECCION,
    DEMANDA, SIMULACION, CLIMA, EDIFICIO, FASE, COMPOSICION, ESTRUCTURA,
    GEOMETRIA, DIMENSIONES, CALZADA,
    PROP,   // tipo, puntos, carriles, ancho_carril, posicion, duracion, ...
    VALOR,  // avenida, doble, verde, lluvia, media, autos, buses, ...
    UNIDAD, // m, km, kmh, s, min, h, veh_h, grados
    ID, NUM, CADENA, IGUAL,
    APAR, CPAR, ALLAVE, CLLAVE, ACORCHETE, CCORCHETE, COMA, PORCIENTO, MENOS,
    FIN,
    NO_SOPORTADO // token valido del lexer pero fuera del automata
};

static std::string NombreSim(Sim s)
{
    switch (s)
    {
    case Sim::VAR: return "VAR";
    case Sim::ESCENARIO: return "ESCENARIO";
    case Sim::MAPA: return "MAPA";
    case Sim::VIA: return "VIA";
    case Sim::PUENTE: return "PUENTE";
    case Sim::EVENTO: return "EVENTO";
    case Sim::SEMAFORO: return "SEMAFORO";
    case Sim::INTERSECCION: return "INTERSECCION";
    case Sim::DEMANDA: return "DEMANDA";
    case Sim::SIMULACION: return "SIMULACION";
    case Sim::CLIMA: return "CLIMA";
    case Sim::EDIFICIO: return "EDIFICIO";
    case Sim::FASE: return "FASE";
    case Sim::COMPOSICION: return "COMPOSICION";
    case Sim::ESTRUCTURA: return "ESTRUCTURA";
    case Sim::GEOMETRIA: return "GEOMETRIA";
    case Sim::DIMENSIONES: return "DIMENSIONES";
    case Sim::CALZADA: return "CALZADA";
    case Sim::PROP: return "PROP";
    case Sim::VALOR: return "VALOR";
    case Sim::UNIDAD: return "UNIDAD";
    case Sim::ID: return "ID";
    case Sim::NUM: return "NUM";
    case Sim::CADENA: return "CADENA";
    case Sim::IGUAL: return "IGUAL";
    case Sim::APAR: return "APAR";
    case Sim::CPAR: return "CPAR";
    case Sim::ALLAVE: return "ALLAVE";
    case Sim::CLLAVE: return "CLLAVE";
    case Sim::ACORCHETE: return "ACORCHETE";
    case Sim::CCORCHETE: return "CCORCHETE";
    case Sim::COMA: return "COMA";
    case Sim::PORCIENTO: return "PORCIENTO";
    case Sim::MENOS: return "MENOS";
    case Sim::FIN: return "FIN";
    case Sim::NO_SOPORTADO: return "NO_SOPORTADO";
    }
    return "?";
}

// Token del lexer -> simbolo del automata
static Sim Clasificar(Tok t)
{
    switch (t)
    {
    case Tok::VAR_KW: return Sim::VAR;
    case Tok::ESCENARIO: return Sim::ESCENARIO;
    case Tok::MAPA: return Sim::MAPA;
    case Tok::VIA: return Sim::VIA;
    case Tok::PUENTE: return Sim::PUENTE;
    case Tok::EVENTO: return Sim::EVENTO;
    case Tok::SEMAFORO: return Sim::SEMAFORO;
    case Tok::INTERSECCION: return Sim::INTERSECCION;
    case Tok::DEMANDA: return Sim::DEMANDA;
    case Tok::SIMULACION: return Sim::SIMULACION;
    case Tok::CLIMA: return Sim::CLIMA;
    case Tok::EDIFICIO: return Sim::EDIFICIO;
    case Tok::FASE: return Sim::FASE;
    case Tok::COMPOSICION: return Sim::COMPOSICION;
    case Tok::ESTRUCTURA: return Sim::ESTRUCTURA;
    case Tok::GEOMETRIA: return Sim::GEOMETRIA;
    case Tok::DIMENSIONES: return Sim::DIMENSIONES;
    case Tok::CALZADA: return Sim::CALZADA;

    // Propiedades
    case Tok::TIPO: case Tok::PUNTOS: case Tok::CARRILES:
    case Tok::ANCHO_CARRIL: case Tok::SENTIDO: case Tok::VELOCIDAD_MAX:
    case Tok::CAPACIDAD: case Tok::POSICION: case Tok::CONECTA:
    case Tok::CICLO: case Tok::DURACION: case Tok::LARGO: case Tok::ANCHO:
    case Tok::ALTURA: case Tok::ROTACION: case Tok::ORIENTACION:
    case Tok::LONGITUD: case Tok::SOBRE:
    case Tok::TABLERO: case Tok::APOYO: case Tok::FLUJO: case Tok::INICIO:
    case Tok::ESTADO: case Tok::INTENSIDAD: case Tok::VISIBILIDAD:
    case Tok::CARRIL: case Tok::INTERVALO: case Tok::REPETICIONES:
        return Sim::PROP;

    // Valores enumerados
    case Tok::AVENIDA: case Tok::CALLE: case Tok::DOBLE: case Tok::UNICO:
    case Tok::VEHICULAR: case Tok::VERDE: case Tok::AMARILLO: case Tok::ROJO:
    case Tok::LLUVIA: case Tok::DESPEJADO: case Tok::BAJA: case Tok::MEDIA:
    case Tok::ALTA: case Tok::ACCIDENTE: case Tok::AUTO: case Tok::AUTOS:
    case Tok::BUS: case Tok::BUSES: case Tok::MOTOS: case Tok::TAXI:
    case Tok::CAMIONES:
        return Sim::VALOR;

    // Unidades
    case Tok::UNIDAD_M: case Tok::UNIDAD_KM: case Tok::UNIDAD_KMH:
    case Tok::UNIDAD_S: case Tok::UNIDAD_MIN: case Tok::UNIDAD_H:
    case Tok::UNIDAD_VEHH: case Tok::UNIDAD_GRADOS:
        return Sim::UNIDAD;

    case Tok::ID: return Sim::ID;
    case Tok::NUM: return Sim::NUM;
    case Tok::CADENA: return Sim::CADENA;
    case Tok::IGUAL: return Sim::IGUAL;
    case Tok::APAR: return Sim::APAR;
    case Tok::CPAR: return Sim::CPAR;
    case Tok::ALLAVE: return Sim::ALLAVE;
    case Tok::CLLAVE: return Sim::CLLAVE;
    case Tok::ACORCHETE: return Sim::ACORCHETE;
    case Tok::CCORCHETE: return Sim::CCORCHETE;
    case Tok::COMA: return Sim::COMA;
    case Tok::PORCIENTO: return Sim::PORCIENTO;
    case Tok::MENOS: return Sim::MENOS;
    case Tok::FIN: return Sim::FIN;

    default: // function, object, if, for, operadores, PUNTO, ...
        return Sim::NO_SOPORTADO;
    }
}

// Estados del PDA (mismos nombres que en el .jff)
enum class Est : int
{
    q0, qv1, qv2, qv3, qe1, qe2, qItem, qAbreB, qHdr, qRef,
    qFase, qAbreH, qProp, qLst, qB1, qTa, qTb, qNeg, qNegT, qAcepta
};

static const char *NombreEst(Est e)
{
    static const char *n[] = {"q0", "qv1", "qv2", "qv3", "qe1", "qe2",
                              "qItem", "qAbreB", "qHdr", "qRef", "qFase",
                              "qAbreH", "qProp", "qLst", "qB1", "qTa",
                              "qTb", "qNeg", "qNegT", "qAcepta"};
    return n[static_cast<int>(e)];
}

struct Transicion
{
    Est desde;
    Sim leer;
    char pop;   // 0 = lambda (no consulta la pila)
    char push;  // 0 = lambda (no apila)
    Est hasta;
};

struct Paso
{
    int paso;
    Est estadoActual;
    std::string clase;               // nombre del simbolo / clase de token
    std::optional<std::string> lexema;
    int linea, columna;
    std::string pilaAntes, pilaDespues;
    std::optional<Est> estadoSiguiente;
};

struct ErrorSintactico
{
    std::string tipo; // "lexico" o "sintactico"
    int linea, columna;
    Est estado;
    std::string mensaje;
};

class AnalizadorSintactico
{
private:
    Tokenizador &tokenizador;
    bool seguimientoActivo;
    Est estadoActual = Est::q0;
    std::string pila = "Z";
    std::vector<Paso> seguimiento;
    std::optional<ErrorSintactico> error;
    std::vector<Transicion> transiciones;

    void T(Est d, Sim l, char pop, char push, Est h)
    {
        transiciones.push_back({d, l, pop, push, h});
    }

    const Transicion *Buscar(Est e, Sim s) const
    {
        for (const auto &t : transiciones)
        {
            if (t.desde != e || t.leer != s)
                continue;
            if (t.pop == 0 || (!pila.empty() && pila.back() == t.pop))
                return &t;
        }
        return nullptr;
    }

    // Simbolos que el automata aceptaria en este estado con el tope actual
    std::string Esperados(Est e) const
    {
        std::string r;
        std::set<Sim> vistos;
        for (const auto &t : transiciones)
        {
            if (t.desde != e)
                continue;
            if (t.pop != 0 && (pila.empty() || pila.back() != t.pop))
                continue;
            if (!vistos.insert(t.leer).second)
                continue;
            if (!r.empty())
                r += ", ";
            r += NombreSim(t.leer);
        }
        return r.empty() ? "ninguno" : r;
    }

    bool ExisteSimboloEnEstado(Est e, Sim s) const
    {
        for (const auto &t : transiciones)
            if (t.desde == e && t.leer == s)
                return true;
        return false;
    }

    std::string Describir(Est e, Sim s) const
    {
        if (s == Sim::NO_SOPORTADO)
            return "Esta construccion existe en el lexer pero no forma parte "
                   "del automata (function, object, if, for, expresiones...).";

        if (s == Sim::FIN && pila.size() > 1)
            return "El programa termino con delimitadores sin cerrar "
                   "(pila = " + pila + "): falta al menos una llave o corchete de cierre.";

        if (ExisteSimboloEnEstado(e, s))
            return std::string("El simbolo es valido en este estado, pero no con el "
                               "tope de pila actual ('") + pila.back() +
                   "'): contexto o delimitador incorrecto.";

        switch (e)
        {
        case Est::q0: return "El programa debe iniciar con 'var' o con 'escenario'.";
        case Est::qv1: return "Despues de 'var' debe ir un identificador.";
        case Est::qv2: return "Despues del identificador de var debe ir '='.";
        case Est::qv3: return "El valor de una variable global debe ser un numero.";
        case Est::qe1: return "Despues de 'escenario' debe ir su nombre entre comillas.";
        case Est::qe2: return "Falta la llave de apertura del escenario.";
        case Est::qHdr: return "Despues de la palabra del elemento debe ir su nombre entre comillas.";
        case Est::qAbreB:
        case Est::qAbreH: return "Falta la llave de apertura del bloque.";
        case Est::qRef: return "Se esperaba un nombre entre comillas.";
        case Est::qFase: return "Despues de 'fase' debe ir un valor (verde, amarillo, rojo).";
        case Est::qProp: return "Despues de la propiedad debe ir un valor, numero, cadena, id, '(' o '['.";
        case Est::qLst: return "En una lista cada elemento es una tupla que inicia con '('.";
        case Est::qB1: return "Despues del valor debe ir un numero (porcentaje).";
        case Est::qTa: return "Se esperaba un numero dentro de la tupla.";
        case Est::qTb: return "En la tupla se esperaba ',' o ')'.";
        case Est::qItem: return "Sentencia no valida en este bloque.";
        default: return "Token sin transicion valida.";
        }
    }

    ErrorSintactico CrearError(const std::string &tipo, const Token &t,
                               Est e, const std::string &desc) const
    {
        std::ostringstream m;
        m << "Error " << tipo << " en linea " << t.linea << ", columna "
          << t.columna << ": recibido '" << (t.lexema ? *t.lexema : "null")
          << "' en el estado " << NombreEst(e) << " (pila = " << pila
          << "); se esperaba: " << Esperados(e) << ". " << desc;
        return {tipo, t.linea, t.columna, e, m.str()};
    }

    void Registrar(int paso, Est ant, const Token &t, const std::string &clase,
                   const std::string &pilaAntes, std::optional<Est> sig)
    {
        if (!seguimientoActivo)
            return;
        seguimiento.push_back({paso, ant, clase, t.lexema, t.linea, t.columna,
                               pilaAntes, pila, sig});
    }

public:
    AnalizadorSintactico(Tokenizador &tk, bool seguimiento = true)
        : tokenizador(tk), seguimientoActivo(seguimiento)
    {
        using E = Est;
        using S = Sim;

        // ---- nivel superior (pila Z) ----
        T(E::q0, S::VAR, 0, 0, E::qv1);
        T(E::qv1, S::ID, 0, 0, E::qv2);
        T(E::qv2, S::IGUAL, 0, 0, E::qv3);
        T(E::qv3, S::NUM, 0, 0, E::q0);
        T(E::q0, S::FIN, 'Z', 'Z', E::qAcepta);       // AGREGADA: programa sin escenario (lo reporta el semantico E-ESC-02)
        T(E::q0, S::ESCENARIO, 0, 0, E::qe1);
        T(E::qe1, S::CADENA, 0, 0, E::qe2);
        T(E::qe2, S::ALLAVE, 0, 'E', E::qItem);

        // ---- HUB qItem: el tope de pila decide ----
        // cierres
        T(E::qItem, S::CLLAVE, 'B', 0, E::qItem);
        T(E::qItem, S::CLLAVE, 'E', 0, E::qItem);
        T(E::qItem, S::CLLAVE, 'H', 0, E::qItem);
        T(E::qItem, S::CCORCHETE, 'K', 0, E::qItem);
        T(E::qItem, S::FIN, 'Z', 'Z', E::qAcepta);

        // elementos dentro del escenario (tope E)
        T(E::qItem, S::MAPA, 'E', 'E', E::qRef);
        T(E::qItem, S::VIA, 'E', 'E', E::qHdr);
        T(E::qItem, S::PUENTE, 'E', 'E', E::qHdr);
        T(E::qItem, S::EVENTO, 'E', 'E', E::qHdr);
        T(E::qItem, S::SEMAFORO, 'E', 'E', E::qHdr);
        T(E::qItem, S::INTERSECCION, 'E', 'E', E::qHdr);
        T(E::qItem, S::DEMANDA, 'E', 'E', E::qHdr);
        T(E::qItem, S::EDIFICIO, 'E', 'E', E::qHdr);      // AGREGADA (no estaba en el .jff)
        T(E::qItem, S::SIMULACION, 'E', 'E', E::qAbreB);
        T(E::qItem, S::CLIMA, 'E', 'E', E::qAbreB);

        // dentro de un bloque (tope B)
        T(E::qItem, S::VIA, 'B', 'B', E::qRef);
        T(E::qItem, S::INTERSECCION, 'B', 'B', E::qRef);
        T(E::qItem, S::FASE, 'B', 'B', E::qFase);
        T(E::qItem, S::COMPOSICION, 'B', 'B', E::qAbreH);
        T(E::qItem, S::ESTRUCTURA, 'B', 'B', E::qAbreH);
        T(E::qItem, S::GEOMETRIA, 'B', 'B', E::qAbreH);
        T(E::qItem, S::DIMENSIONES, 'B', 'B', E::qAbreH); // AGREGADA (no estaba en el .jff)
        T(E::qItem, S::CALZADA, 'B', 'B', E::qAbreH);     // AGREGADA: sub-bloque calzada del puente (ej. 15-16)
        T(E::qItem, S::SEMAFORO, 'B', 'B', E::qHdr);      // AGREGADA: semaforo anidado en interseccion (ej. 9); el semantico exige que el padre sea 'interseccion'
        T(E::qItem, S::PROP, 'B', 'B', E::qProp);
        T(E::qItem, S::UNIDAD, 'B', 'B', E::qItem);
        T(E::qItem, S::PORCIENTO, 'B', 'B', E::qItem);

        // dentro de un sub-bloque (tope H)
        T(E::qItem, S::PROP, 'H', 'H', E::qProp);
        T(E::qItem, S::VALOR, 'H', 'H', E::qB1);
        T(E::qItem, S::UNIDAD, 'H', 'H', E::qItem);
        T(E::qItem, S::PORCIENTO, 'H', 'H', E::qItem);

        // dentro de una lista (tope K)
        T(E::qItem, S::COMA, 'K', 'K', E::qLst);

        // ---- cabeceras y llaves ----
        T(E::qHdr, S::CADENA, 0, 0, E::qAbreB);
        T(E::qAbreB, S::ALLAVE, 0, 'B', E::qItem);
        T(E::qRef, S::CADENA, 0, 0, E::qItem);
        T(E::qFase, S::VALOR, 0, 0, E::qAbreH);
        T(E::qFase, S::ID, 0, 0, E::qAbreH);           // AGREGADA: nombre de fase invalido (lo reporta el semantico E-DOM-01)
        T(E::qAbreH, S::ALLAVE, 0, 'H', E::qItem);
        T(E::qB1, S::NUM, 0, 0, E::qItem);

        // ---- propiedad: valor ----
        T(E::qProp, S::VALOR, 0, 0, E::qItem);
        T(E::qProp, S::NUM, 0, 0, E::qItem);
        T(E::qProp, S::ID, 0, 0, E::qItem);
        T(E::qProp, S::CADENA, 0, 0, E::qItem);
        T(E::qProp, S::APAR, 0, 'P', E::qTa);
        T(E::qProp, S::ACORCHETE, 0, 'K', E::qLst);

        // ---- listas y tuplas ----
        T(E::qLst, S::APAR, 0, 'P', E::qTa);
        T(E::qTa, S::NUM, 0, 0, E::qTb);
        T(E::qTb, S::COMA, 0, 0, E::qTa);
        T(E::qTb, S::CPAR, 'P', 0, E::qItem);
        // ---- AGREGADAS: tuplas de una lista sin coma entre ellas (ej. 3) ----
        T(E::qItem, S::APAR, 'K', 'K', E::qTa);
        T(E::qTb, S::CPAR, 'K', 'K', E::qItem);

        // ---- AGREGADAS: numeros negativos (el rango lo valida el semantico E-DOM-08) ----
        T(E::qProp, S::MENOS, 0, 0, E::qNeg);
        T(E::qB1, S::MENOS, 0, 0, E::qNeg);
        T(E::qNeg, S::NUM, 0, 0, E::qItem);
        T(E::qTa, S::MENOS, 0, 0, E::qNegT);
        T(E::qNegT, S::NUM, 0, 0, E::qTb);
    }

    bool Analizar()
    {
        estadoActual = Est::q0;
        pila = "Z";
        seguimiento.clear();
        error.reset();
        int paso = 1;

        while (true)
        {
            Token token = tokenizador.getToken();
            Est anterior = estadoActual;
            std::string pilaAntes = pila;

            if (token.token == Tok::ERROR)
            {
                Registrar(paso, anterior, token, "ERROR", pilaAntes, std::nullopt);
                error = CrearError("lexico", token, anterior,
                                   "El analizador lexico encontro un lexema no valido.");
                return false;
            }

            Sim sim = Clasificar(token.token);
            const Transicion *tr = Buscar(estadoActual, sim);

            if (!tr)
            {
                Registrar(paso, anterior, token, NombreSim(sim), pilaAntes, std::nullopt);
                error = CrearError("sintactico", token, anterior,
                                   Describir(anterior, sim));
                return false;
            }

            // operacion de pila: pop (si hay) y luego push (si hay)
            if (tr->pop != 0)
                pila.pop_back();
            if (tr->push != 0)
                pila.push_back(tr->push);

            estadoActual = tr->hasta;
            Registrar(paso, anterior, token, NombreSim(sim), pilaAntes, estadoActual);

            if (estadoActual == Est::qAcepta)
                return true;

            paso++;
        }
    }

    const std::vector<Paso> &getSeguimiento() const { return seguimiento; }
    const std::optional<ErrorSintactico> &getError() const { return error; }
    Est getEstadoActual() const { return estadoActual; }
};


/* ============================================================
 * ANALIZADOR SEMANTICO
 * ------------------------------------------------------------
 * Se ejecuta DESPUES del sintactico (solo sobre programas que el
 * automata de pila acepto). Trabaja en dos pasos:
 *
 *   1) ConstructorAST: recorre los tokens y arma un arbol
 *      (variables, escenarios, bloques, propiedades, referencias,
 *      sub-bloques). Como el sintactico ya valido la estructura,
 *      este paso no reporta errores.
 *   2) AnalizadorSemantico: aplica el catalogo de errores
 *      semanticos (E-VAR, E-ESC, E-STR, E-REF, E-DOM, E-TIP, E-UNI).
 *
 * Los errores NO detienen el analisis: se acumulan todos y se
 * ordenan por linea/columna.
 * ============================================================ */

struct Pos
{
    int linea = 0;
    int columna = 0;
};

// Valor que sigue a una propiedad: NUM, VALOR (enum), CADENA, ID, tupla o lista
struct Operando
{
    enum class Tipo { NINGUNO, NUM, VALOR, CADENA, ID, TUPLA, LISTA };
    Tipo tipo = Tipo::NINGUNO;
    double num = 0;
    bool decimal = false;                      // el literal tenia '.'
    std::string texto;                         // VALOR / CADENA / ID
    std::vector<std::vector<double>> tuplas;   // TUPLA (1 elemento) o LISTA (n)
    std::vector<Pos> posTuplas;
    std::vector<std::string> unidades;         // "m", "kmh", "%", ...
    std::vector<Pos> posUnidades;
};

struct PropAST
{
    Tok tok;
    std::string nombre;
    Operando op;
    Pos pos;
};

struct RefAST // via "X" / interseccion "X" dentro de un bloque, o mapa "X"
{
    Tok tok;
    std::string nombre;
    Pos pos; // posicion del nombre (o de la palabra si no hay nombre)
};

struct CategoriaAST // autos 60 %  (VALOR NUM [unidad])
{
    Tok tok;
    std::string nombre;
    double num = 0;
    std::vector<std::string> unidades;
    Pos pos;
};

struct SueltoAST // unidad o '%' que no sigue a ningun valor
{
    std::string lexema;
    Pos pos;
};

struct BloqueAST
{
    Tok tipo = Tok::ERROR;
    std::string nombre; // identificador de la entidad (o nombre de fase)
    bool conNombre = false;
    Pos pos, posNombre;
    std::vector<PropAST> props;
    std::vector<RefAST> refs;
    std::vector<CategoriaAST> categorias;
    std::vector<SueltoAST> sueltos;
    std::vector<BloqueAST> hijos;

    bool Vacio() const
    {
        return props.empty() && refs.empty() && categorias.empty() &&
               sueltos.empty() && hijos.empty();
    }
};

struct VarAST
{
    std::string nombre;
    double valor = 0;
    bool entero = true;
    Pos pos;
};

struct EscenarioAST
{
    std::string nombre;
    Pos pos, posNombre;
    std::vector<RefAST> mapas;
    std::vector<BloqueAST> bloques;
};

struct ProgramaAST
{
    std::vector<VarAST> vars;
    std::vector<EscenarioAST> escenarios;
    Pos posFin;
};

/* ------------------------------------------------------------
 * Paso 1: tokens -> AST
 * ------------------------------------------------------------ */
class ConstructorAST
{
private:
    std::vector<Token> toks; // siempre termina en FIN
    std::size_t i = 0;

    const Token &Act() const { return toks[i]; }
    bool Es(Tok t) const { return toks[i].token == t; }
    Sim Cl() const { return Clasificar(toks[i].token); }

    Token Tomar()
    {
        Token t = toks[i];
        if (toks[i].token != Tok::FIN)
            i++;
        return t;
    }

    static Pos P(const Token &t) { return {t.linea, t.columna}; }
    static std::string L(const Token &t) { return t.lexema ? *t.lexema : ""; }

    std::pair<double, bool> Numero() // (valor con signo, era decimal)
    {
        bool neg = false;
        if (Es(Tok::MENOS))
        {
            Tomar();
            neg = true;
        }
        Token n = Tomar();
        std::string s = L(n);
        double v = std::strtod(s.c_str(), nullptr);
        return {neg ? -v : v, s.find('.') != std::string::npos};
    }

    std::vector<double> Tupla(Pos &pos)
    {
        pos = P(Act());
        Tomar(); // (
        std::vector<double> r;
        while (!Es(Tok::FIN) && !Es(Tok::CPAR))
        {
            if (Es(Tok::COMA))
            {
                Tomar();
                continue;
            }
            r.push_back(Numero().first);
        }
        Tomar(); // )
        return r;
    }

    void Unidades(std::vector<std::string> &u, std::vector<Pos> *pos)
    {
        while (Cl() == Sim::UNIDAD || Cl() == Sim::PORCIENTO)
        {
            Token t = Tomar();
            u.push_back(L(t));
            if (pos)
                pos->push_back(P(t));
        }
    }

    PropAST Prop()
    {
        Token p = Tomar();
        PropAST r;
        r.tok = p.token;
        r.nombre = L(p);
        r.pos = P(p);
        Operando &o = r.op;

        if (Es(Tok::MENOS) || Es(Tok::NUM))
        {
            o.tipo = Operando::Tipo::NUM;
            auto n = Numero();
            o.num = n.first;
            o.decimal = n.second;
        }
        else if (Cl() == Sim::VALOR)
        {
            o.tipo = Operando::Tipo::VALOR;
            o.texto = L(Tomar());
        }
        else if (Es(Tok::ID))
        {
            o.tipo = Operando::Tipo::ID;
            o.texto = L(Tomar());
        }
        else if (Es(Tok::CADENA))
        {
            o.tipo = Operando::Tipo::CADENA;
            o.texto = L(Tomar());
        }
        else if (Es(Tok::APAR))
        {
            o.tipo = Operando::Tipo::TUPLA;
            Pos pt;
            o.tuplas.push_back(Tupla(pt));
            o.posTuplas.push_back(pt);
        }
        else if (Es(Tok::ACORCHETE))
        {
            o.tipo = Operando::Tipo::LISTA;
            Tomar(); // [
            while (!Es(Tok::FIN) && !Es(Tok::CCORCHETE))
            {
                if (Es(Tok::APAR))
                {
                    Pos pt;
                    o.tuplas.push_back(Tupla(pt));
                    o.posTuplas.push_back(pt);
                }
                else
                    Tomar(); // comas
            }
            Tomar(); // ]
        }

        Unidades(o.unidades, &o.posUnidades);
        return r;
    }

    // Cuerpo de un bloque B o sub-bloque H (hasta la '}' sin consumirla)
    void Bloque(BloqueAST &b)
    {
        while (!Es(Tok::FIN) && !Es(Tok::CLLAVE))
        {
            switch (Cl())
            {
            case Sim::VIA:
            case Sim::INTERSECCION:
            {
                Token t = Tomar();
                RefAST r{t.token, "", P(t)};
                if (Es(Tok::CADENA))
                {
                    Token n = Tomar();
                    r.nombre = L(n);
                    r.pos = P(n);
                }
                b.refs.push_back(r);
                break;
            }
            case Sim::FASE:
            {
                Token t = Tomar();
                BloqueAST h;
                h.tipo = Tok::FASE;
                h.pos = P(t);
                if (Cl() == Sim::VALOR || Es(Tok::ID))
                {
                    Token n = Tomar();
                    h.nombre = L(n);
                    h.conNombre = true;
                    h.posNombre = P(n);
                }
                if (Es(Tok::ALLAVE))
                    Tomar();
                Bloque(h);
                if (Es(Tok::CLLAVE))
                    Tomar();
                b.hijos.push_back(std::move(h));
                break;
            }
            case Sim::COMPOSICION:
            case Sim::ESTRUCTURA:
            case Sim::GEOMETRIA:
            case Sim::DIMENSIONES:
            case Sim::CALZADA:
            {
                Token t = Tomar();
                BloqueAST h;
                h.tipo = t.token;
                h.pos = P(t);
                if (Es(Tok::ALLAVE))
                    Tomar();
                Bloque(h);
                if (Es(Tok::CLLAVE))
                    Tomar();
                b.hijos.push_back(std::move(h));
                break;
            }
            case Sim::SEMAFORO: // semaforo anidado (dentro de interseccion)
            {
                Token t = Tomar();
                BloqueAST h;
                h.tipo = t.token;
                h.pos = P(t);
                h.posNombre = P(t);
                if (Es(Tok::CADENA))
                {
                    Token n = Tomar();
                    h.nombre = L(n);
                    h.conNombre = true;
                    h.posNombre = P(n);
                }
                if (Es(Tok::ALLAVE))
                    Tomar();
                Bloque(h);
                if (Es(Tok::CLLAVE))
                    Tomar();
                b.hijos.push_back(std::move(h));
                break;
            }
            case Sim::PROP:
                b.props.push_back(Prop());
                break;
            case Sim::VALOR:
            {
                Token t = Tomar();
                CategoriaAST c;
                c.tok = t.token;
                c.nombre = L(t);
                c.pos = P(t);
                if (Es(Tok::MENOS) || Es(Tok::NUM))
                    c.num = Numero().first;
                Unidades(c.unidades, nullptr);
                b.categorias.push_back(c);
                break;
            }
            case Sim::UNIDAD:
            case Sim::PORCIENTO:
            {
                Token t = Tomar();
                b.sueltos.push_back({L(t), P(t)});
                break;
            }
            default:
                Tomar();
            }
        }
    }

    EscenarioAST Escenario()
    {
        Token e = Tomar();
        EscenarioAST esc;
        esc.pos = P(e);
        esc.posNombre = P(e);
        if (Es(Tok::CADENA))
        {
            Token n = Tomar();
            esc.nombre = L(n);
            esc.posNombre = P(n);
        }
        if (Es(Tok::ALLAVE))
            Tomar();

        while (!Es(Tok::FIN) && !Es(Tok::CLLAVE))
        {
            switch (Cl())
            {
            case Sim::MAPA:
            {
                Token t = Tomar();
                RefAST r{t.token, "", P(t)};
                if (Es(Tok::CADENA))
                {
                    Token n = Tomar();
                    r.nombre = L(n);
                    r.pos = P(n);
                }
                esc.mapas.push_back(r);
                break;
            }
            case Sim::VIA:
            case Sim::PUENTE:
            case Sim::EVENTO:
            case Sim::SEMAFORO:
            case Sim::INTERSECCION:
            case Sim::DEMANDA:
            case Sim::EDIFICIO:
            case Sim::SIMULACION:
            case Sim::CLIMA:
            {
                Token t = Tomar();
                BloqueAST b;
                b.tipo = t.token;
                b.pos = P(t);
                b.posNombre = P(t);
                if (Es(Tok::CADENA))
                {
                    Token n = Tomar();
                    b.nombre = L(n);
                    b.conNombre = true;
                    b.posNombre = P(n);
                }
                if (Es(Tok::ALLAVE))
                    Tomar();
                Bloque(b);
                if (Es(Tok::CLLAVE))
                    Tomar();
                esc.bloques.push_back(std::move(b));
                break;
            }
            default:
                Tomar();
            }
        }
        if (Es(Tok::CLLAVE))
            Tomar();
        return esc;
    }

public:
    explicit ConstructorAST(std::vector<Token> tokens) : toks(std::move(tokens)) {}

    ProgramaAST Construir()
    {
        ProgramaAST prog;
        while (!Es(Tok::FIN))
        {
            if (Es(Tok::VAR_KW))
            {
                Tomar();
                Token id = Tomar();
                Tomar(); // =
                Token n = Tomar();
                VarAST v;
                v.nombre = L(id);
                v.pos = P(id);
                v.valor = std::strtod(L(n).c_str(), nullptr);
                v.entero = L(n).find('.') == std::string::npos;
                prog.vars.push_back(v);
            }
            else if (Es(Tok::ESCENARIO))
                prog.escenarios.push_back(Escenario());
            else
                Tomar();
        }
        prog.posFin = P(Act());
        return prog;
    }
};

/* ------------------------------------------------------------
 * Especificacion de propiedades (tipo, unidad, rango, dominio)
 * ------------------------------------------------------------ */
enum class Clase { ENTERO, REAL, ENUM, REFERENCIA, TUPLA3, PUNTOS };
enum class Rango { LIBRE, POSITIVO, NO_NEGATIVO, MIN_UNO, GRADOS, PORCENTAJE };

struct EspecProp
{
    Clase clase;
    std::string unidad; // "" = adimensional
    Rango rango;
    std::vector<std::string> dominio; // solo ENUM
};

// Devuelve la especificacion de la propiedad 'p' dentro del bloque 'b',
// o nullopt si esa propiedad no existe en ese bloque.
static std::optional<EspecProp> BuscarEspec(Tok p, Tok b)
{
    using C = Clase;
    using R = Rango;
    auto S = [](C c, const char *u, R r, std::vector<std::string> d = {})
    {
        return std::optional<EspecProp>(EspecProp{c, u, r, std::move(d)});
    };

    switch (p)
    {
    case Tok::TIPO:
        if (b == Tok::VIA) return S(C::ENUM, "", R::LIBRE, {"avenida", "calle"});
        if (b == Tok::PUENTE) return S(C::ENUM, "", R::LIBRE, {"vehicular"});
        if (b == Tok::EVENTO) return S(C::ENUM, "", R::LIBRE, {"accidente"});
        break;
    case Tok::PUNTOS:
        if (b == Tok::VIA) return S(C::PUNTOS, "", R::LIBRE);
        break;
    case Tok::CARRILES:
        if ((b == Tok::VIA || b == Tok::CALZADA)) return S(C::ENTERO, "", R::POSITIVO);
        break;
    case Tok::ANCHO_CARRIL:
        if ((b == Tok::VIA || b == Tok::CALZADA)) return S(C::REAL, "m", R::POSITIVO);
        break;
    case Tok::SENTIDO:
        if ((b == Tok::VIA || b == Tok::CALZADA)) return S(C::ENUM, "", R::LIBRE, {"doble", "unico"});
        break;
    case Tok::VELOCIDAD_MAX:
        if ((b == Tok::VIA || b == Tok::CALZADA)) return S(C::REAL, "kmh", R::POSITIVO);
        break;
    case Tok::CAPACIDAD:
        if ((b == Tok::VIA || b == Tok::CALZADA)) return S(C::REAL, "veh_h", R::POSITIVO);
        break;
    case Tok::POSICION:
        if (b == Tok::INTERSECCION || b == Tok::EDIFICIO || b == Tok::PUENTE)
            return S(C::TUPLA3, "", R::LIBRE);
        break;
    case Tok::CONECTA:
        if (b == Tok::INTERSECCION) return S(C::REFERENCIA, "", R::LIBRE);
        break;
    case Tok::CICLO:
        if (b == Tok::SEMAFORO) return S(C::REAL, "s", R::POSITIVO);
        break;
    case Tok::DURACION:
        if (b == Tok::FASE) return S(C::REAL, "s", R::POSITIVO);
        if (b == Tok::DEMANDA || b == Tok::EVENTO || b == Tok::SIMULACION)
            return S(C::REAL, "min", R::POSITIVO);
        break;
    case Tok::ROTACION:
        if (b == Tok::EDIFICIO) return S(C::REAL, "grados", R::GRADOS);
        break;
    case Tok::ORIENTACION:
        if (b == Tok::PUENTE) return S(C::REAL, "grados", R::GRADOS);
        break;
    case Tok::LARGO:
        if (b == Tok::DIMENSIONES) return S(C::REAL, "m", R::POSITIVO);
        break;
    case Tok::ANCHO:
    case Tok::ALTURA:
        if (b == Tok::DIMENSIONES || b == Tok::GEOMETRIA)
            return S(C::REAL, "m", R::POSITIVO);
        break;
    case Tok::LONGITUD:
        if (b == Tok::GEOMETRIA) return S(C::REAL, "m", R::POSITIVO);
        break;
    case Tok::SOBRE:
        if (b == Tok::PUENTE) return S(C::REFERENCIA, "", R::LIBRE);
        break;
    case Tok::TABLERO:
        if (b == Tok::ESTRUCTURA) return S(C::REAL, "m", R::POSITIVO);
        break;
    case Tok::APOYO:
        if (b == Tok::ESTRUCTURA) return S(C::ENTERO, "", R::NO_NEGATIVO);
        break;
    case Tok::FLUJO:
        if (b == Tok::DEMANDA) return S(C::REAL, "veh_h", R::POSITIVO);
        break;
    case Tok::INICIO:
        if (b == Tok::DEMANDA || b == Tok::EVENTO)
            return S(C::REAL, "min", R::NO_NEGATIVO);
        break;
    case Tok::ESTADO:
        if (b == Tok::CLIMA) return S(C::ENUM, "", R::LIBRE, {"lluvia", "despejado"});
        break;
    case Tok::INTENSIDAD:
        if (b == Tok::CLIMA) return S(C::ENUM, "", R::LIBRE, {"baja", "media", "alta"});
        break;
    case Tok::VISIBILIDAD:
        if (b == Tok::CLIMA) return S(C::REAL, "%", R::PORCENTAJE);
        break;
    case Tok::CARRIL:
        if (b == Tok::EVENTO) return S(C::ENTERO, "", R::MIN_UNO);
        break;
    case Tok::INTERVALO:
        if (b == Tok::SIMULACION) return S(C::REAL, "s", R::POSITIVO);
        break;
    case Tok::REPETICIONES:
        if (b == Tok::SIMULACION) return S(C::ENTERO, "", R::POSITIVO);
        break;
    default:
        break;
    }
    return std::nullopt;
}

/* ------------------------------------------------------------
 * Paso 2: reglas semanticas
 * ------------------------------------------------------------ */
struct ErrorSemantico
{
    std::string codigo;
    int linea, columna;
    std::string mensaje;
};

// Valor ya validado de una propiedad (para las reglas cruzadas)
struct Medida
{
    double v = 0;
    std::string u;
    Pos pos;
};

struct Evaluado
{
    std::map<std::string, Medida> p;                    // propiedades validas del bloque
    std::map<Tok, std::map<std::string, Medida>> sub;   // propiedades validas de sub-bloques
    std::vector<Medida> fases;                          // duracion de cada fase (s)
    bool fasesOk = true;                                // todas las fases evaluables
};

class AnalizadorSemantico
{
private:
    const TablaLexica &tabla;
    std::vector<ErrorSemantico> errores;
    std::map<std::string, VarAST> vars;
    std::set<std::string> viasDecl, intersDecl;
    std::vector<std::pair<const BloqueAST *, Evaluado>> anidados; // semaforos dentro de interseccion

    void Err(const std::string &codigo, Pos p, const std::string &msg)
    {
        errores.push_back({codigo, p.linea, p.columna, msg});
    }

    std::string Nombre(Tok t) const
    {
        auto e = tabla.BuscarPorToken(t);
        return e ? e->lexema : "?";
    }

    static std::string Fmt(double v)
    {
        std::ostringstream o;
        o << std::setprecision(10) << v;
        return o.str();
    }

    static double Seg(const Medida &m)
    {
        if (m.u == "min") return m.v * 60.0;
        if (m.u == "h") return m.v * 3600.0;
        return m.v;
    }

    static bool EnRango(double v, Rango r)
    {
        switch (r)
        {
        case Rango::POSITIVO: return v > 0;
        case Rango::NO_NEGATIVO: return v >= 0;
        case Rango::MIN_UNO: return v >= 1;
        case Rango::GRADOS: return v >= 0 && v <= 360;
        case Rango::PORCENTAJE: return v >= 0 && v <= 100;
        default: return true;
        }
    }

    static const char *TextoRango(Rango r)
    {
        switch (r)
        {
        case Rango::POSITIVO: return "debe ser mayor que 0";
        case Rango::NO_NEGATIVO: return "debe ser mayor o igual que 0";
        case Rango::MIN_UNO: return "debe ser mayor o igual que 1";
        case Rango::GRADOS: return "debe estar en [0, 360] grados";
        case Rango::PORCENTAJE: return "debe estar en [0, 100] %";
        default: return "";
        }
    }

    /* ---------- propiedades ---------- */
    void EvaluarProp(const PropAST &p, Tok bloque, std::map<std::string, Medida> &ok)
    {
        auto esp = BuscarEspec(p.tok, bloque);
        if (!esp)
        {
            Err("E-STR-08", p.pos,
                "La propiedad '" + p.nombre + "' no esta permitida en el bloque '" +
                    Nombre(bloque) + "'.");
            return;
        }

        const Operando &o = p.op;
        using T = Operando::Tipo;

        // Operando que no corresponde a la clase esperada
        auto Incompatible = [&]()
        {
            if (o.tipo == T::ID)
            {
                if (vars.count(o.texto))
                    Err("E-TIP-01", p.pos,
                        "La variable '" + o.texto + "' (numerica) no es compatible con la propiedad '" +
                            p.nombre + "'.");
                else
                    Err("E-VAR-02", p.pos,
                        "El identificador '" + o.texto +
                            "' se usa como valor pero no fue declarado con 'var'.");
            }
            else
                Err("E-TIP-02", p.pos,
                    "El operando no es compatible con la propiedad '" + p.nombre +
                        "' (se esperaba " +
                        (esp->clase == Clase::ENTERO || esp->clase == Clase::REAL ? "un numero"
                         : esp->clase == Clase::ENUM                              ? "un valor de enumeracion"
                         : esp->clase == Clase::REFERENCIA                        ? "una cadena"
                         : esp->clase == Clase::TUPLA3                            ? "una tupla (x, y, z)"
                                                                                  : "una lista de tuplas") +
                        ").");
        };

        double v = 0;
        switch (esp->clase)
        {
        case Clase::ENTERO:
        case Clase::REAL:
            if (o.tipo == T::NUM)
            {
                v = o.num;
                if (esp->clase == Clase::ENTERO && o.decimal)
                {
                    Err("E-TIP-03", p.pos,
                        "La propiedad '" + p.nombre + "' requiere un numero entero, se recibio " +
                            Fmt(o.num) + ".");
                    return;
                }
            }
            else if (o.tipo == T::ID)
            {
                auto it = vars.find(o.texto);
                if (it == vars.end())
                {
                    Incompatible(); // E-VAR-02
                    return;
                }
                if (esp->clase == Clase::ENTERO && !it->second.entero)
                {
                    Err("E-TIP-01", p.pos,
                        "La variable '" + o.texto + "' es decimal (" + Fmt(it->second.valor) +
                            ") y la propiedad '" + p.nombre + "' requiere un entero.");
                    return;
                }
                v = it->second.valor;
            }
            else
            {
                Incompatible();
                return;
            }
            break;

        case Clase::ENUM:
            if (o.tipo == T::VALOR || o.tipo == T::ID)
            {
                if (o.tipo == T::ID && vars.count(o.texto))
                {
                    Incompatible(); // E-TIP-01
                    return;
                }
                if (std::find(esp->dominio.begin(), esp->dominio.end(), o.texto) ==
                    esp->dominio.end())
                {
                    std::string dom;
                    for (const auto &d : esp->dominio)
                        dom += (dom.empty() ? "" : ", ") + d;
                    Err("E-DOM-05", p.pos,
                        "El valor '" + o.texto + "' esta fuera del dominio de '" + p.nombre +
                            "' en '" + Nombre(bloque) + "' (permitidos: " + dom + ").");
                    return;
                }
            }
            else
            {
                Incompatible();
                return;
            }
            break;

        case Clase::REFERENCIA:
            if (o.tipo != T::CADENA)
            {
                Incompatible();
                return;
            }
            break;
        case Clase::TUPLA3:
            if (o.tipo != T::TUPLA)
            {
                Incompatible();
                return;
            }
            break;
        case Clase::PUNTOS:
            if (o.tipo != T::LISTA)
            {
                Incompatible();
                return;
            }
            break;
        }

        // ---- unidades ----
        if (esp->unidad.empty())
        {
            if (!o.unidades.empty())
            {
                Err("E-UNI-02", o.posUnidades[0],
                    "Unidad o simbolo '" + o.unidades[0] + "' inesperado: la propiedad '" +
                        p.nombre + "' es adimensional.");
                return;
            }
        }
        else
        {
            if (o.unidades.empty())
            {
                Err("E-UNI-03", p.pos,
                    "Falta la unidad de medida obligatoria para '" + p.nombre + "' (se espera '" +
                        esp->unidad + "').");
                return;
            }
            if (o.unidades.size() > 1 || o.unidades[0] != esp->unidad)
            {
                std::string dadas;
                for (const auto &u : o.unidades)
                    dadas += (dadas.empty() ? "" : " ") + u;
                Err("E-UNI-04", o.posUnidades[0],
                    "Unidad fisica incorrecta para '" + p.nombre + "': se recibio '" + dadas +
                        "' y se espera '" + esp->unidad + "'.");
                return;
            }
        }

        // ---- reglas por clase ----
        switch (esp->clase)
        {
        case Clase::ENTERO:
        case Clase::REAL:
            if (!EnRango(v, esp->rango))
            {
                Err("E-DOM-08", p.pos,
                    "Valor fuera del rango fisico: '" + p.nombre + "' " + TextoRango(esp->rango) +
                        " (se recibio " + Fmt(v) + (esp->unidad.empty() ? "" : " " + esp->unidad) + ").");
                return;
            }
            ok[p.nombre] = {v, esp->unidad, p.pos};
            break;

        case Clase::TUPLA3:
            if (o.tuplas[0].size() != 3)
                Err("E-TIP-04", o.posTuplas[0],
                    "Dimension de tupla invalida en '" + p.nombre + "': se esperaban 3 componentes (x, y, z) y hay " +
                        std::to_string(o.tuplas[0].size()) + ".");
            break;

        case Clase::PUNTOS:
        {
            for (std::size_t k = 0; k < o.tuplas.size(); k++)
                if (o.tuplas[k].size() != 3)
                    Err("E-TIP-04", o.posTuplas[k],
                        "Dimension de tupla invalida en 'puntos': se esperaban 3 componentes (x, y, z) y hay " +
                            std::to_string(o.tuplas[k].size()) + ".");

            if (o.tuplas.size() < 2)
                Err("E-DOM-06", p.pos,
                    "La lista 'puntos' de una via debe tener al menos 2 coordenadas 3D (tiene " +
                        std::to_string(o.tuplas.size()) + ").");

            for (std::size_t k = 1; k < o.tuplas.size(); k++)
                if (o.tuplas[k].size() == 3 && o.tuplas[k - 1].size() == 3 &&
                    o.tuplas[k] == o.tuplas[k - 1])
                    Err("E-DOM-07", o.posTuplas[k],
                        "Puntos consecutivos identicos en el trazado de la via (puntos " +
                            std::to_string(k) + " y " + std::to_string(k + 1) + ").");
            break;
        }

        case Clase::REFERENCIA:
            if (!viasDecl.count(o.texto))
                Err("E-REF-03", p.pos,
                    "'" + p.nombre + "' referencia a la via \"" + o.texto +
                        "\", que no esta declarada en el escenario.");
            break;

        case Clase::ENUM:
            break;
        }
    }

    /* ---------- referencias dentro de un bloque ---------- */
    void RevisarRef(const BloqueAST &b, const RefAST &r)
    {
        if (r.tok == Tok::VIA)
        {
            if (b.tipo != Tok::DEMANDA && b.tipo != Tok::EVENTO)
                Err("E-STR-05", r.pos,
                    "Referencia 'via' no permitida en '" + Nombre(b.tipo) +
                        "' (solo valida en 'demanda' y 'evento').");
            else if (!viasDecl.count(r.nombre))
                Err("E-REF-03", r.pos,
                    "Referencia a la via \"" + r.nombre + "\", que no esta declarada en el escenario.");
        }
        else // interseccion
        {
            if (b.tipo != Tok::SEMAFORO)
                Err("E-STR-05", r.pos,
                    "Referencia 'interseccion' no permitida en '" + Nombre(b.tipo) +
                        "' (solo valida en 'semaforo').");
            else if (!intersDecl.count(r.nombre))
                Err("E-REF-03", r.pos,
                    "Referencia a la interseccion \"" + r.nombre +
                        "\", que no esta declarada en el escenario.");
        }
    }

    /* ---------- composicion ---------- */
    void Composicion(const BloqueAST &h)
    {
        for (const auto &p : h.props)
            Err("E-STR-07", p.pos,
                "La propiedad de dimension/estructura '" + p.nombre +
                    "' no esta permitida dentro de 'composicion'.");

        double suma = 0;
        bool limpia = true;
        for (const auto &c : h.categorias)
        {
            switch (c.tok)
            {
            case Tok::AUTO: case Tok::AUTOS: case Tok::BUS: case Tok::BUSES:
            case Tok::MOTOS: case Tok::TAXI: case Tok::CAMIONES:
                break;
            default:
                Err("E-DOM-05", c.pos,
                    "'" + c.nombre + "' no es una categoria vehicular valida en 'composicion'.");
                limpia = false;
                continue;
            }
            if (c.unidades.size() != 1 || c.unidades[0] != "%")
            {
                Err("E-UNI-01", c.pos,
                    "La entrada '" + c.nombre + "' de 'composicion' exige estrictamente el simbolo '%'.");
                limpia = false;
                continue;
            }
            if (c.num < 0 || c.num > 100)
            {
                Err("E-DOM-08", c.pos,
                    "Porcentaje fuera de [0, 100] en '" + c.nombre + "' (se recibio " + Fmt(c.num) + " %).");
                limpia = false;
                continue;
            }
            suma += c.num;
        }

        if (limpia && !h.categorias.empty() && std::fabs(suma - 100.0) > 1e-6)
            Err("E-DOM-03", h.pos,
                "La suma de los porcentajes en 'composicion' debe ser exactamente 100 % (suma = " +
                    Fmt(suma) + " %).");
    }

    /* ---------- sub-bloques ---------- */
    void RevisarHijo(const BloqueAST &b, const BloqueAST &h, Evaluado &ev,
                     std::set<std::string> &fasesVistas)
    {
        // compatibilidad padre/hijo
        if (h.tipo == Tok::FASE)
        {
            if (b.tipo != Tok::SEMAFORO)
            {
                Err("E-STR-05", h.pos,
                    "El sub-bloque 'fase' solo es valido dentro de 'semaforo' (aparece en '" +
                        Nombre(b.tipo) + "').");
                return;
            }
        }
        else if (h.tipo == Tok::SEMAFORO)
        {
            if (b.tipo != Tok::INTERSECCION)
            {
                Err("E-STR-05", h.pos,
                    "El sub-bloque 'semaforo' solo es valido dentro de 'interseccion' (aparece en '" +
                        Nombre(b.tipo) + "').");
                return;
            }
            Evaluado sub;
            AnalizarBloque(h, sub);
            anidados.push_back({&h, std::move(sub)});
            return;
        }
        else
        {
            Tok padreOk;
            switch (h.tipo)
            {
            case Tok::COMPOSICION: padreOk = Tok::DEMANDA; break;
            case Tok::DIMENSIONES: padreOk = Tok::EDIFICIO; break;
            default: padreOk = Tok::PUENTE; break; // estructura / geometria / calzada
            }
            if (b.tipo != padreOk)
            {
                Err("E-STR-06", h.pos,
                    "Sub-bloque '" + Nombre(h.tipo) + "' incompatible con el bloque padre '" +
                        Nombre(b.tipo) + "' (solo valido en '" + Nombre(padreOk) + "').");
                return;
            }
        }

        // nombre de fase
        if (h.tipo == Tok::FASE)
        {
            bool valida = h.nombre == "verde" || h.nombre == "amarillo" || h.nombre == "rojo";
            if (!valida)
            {
                Err("E-DOM-01", h.posNombre,
                    "Nombre de fase semaforica invalido '" + h.nombre +
                        "' (debe ser 'verde', 'amarillo' o 'rojo').");
                ev.fasesOk = false;
            }
            else if (!fasesVistas.insert(h.nombre).second)
            {
                Err("E-DOM-02", h.posNombre,
                    "Fase '" + h.nombre + "' duplicada dentro del mismo 'semaforo'.");
                ev.fasesOk = false;
            }
        }

        // cuerpo
        if (h.Vacio())
            Err("E-STR-04", h.pos,
                "Sub-bloque '" + Nombre(h.tipo) + "' vacio; faltan sus propiedades obligatorias.");

        for (const auto &s : h.sueltos)
            Err("E-UNI-02", s.pos,
                "Unidad o simbolo '" + s.lexema + "' inesperado: no sigue a ningun valor.");

        if (h.tipo == Tok::COMPOSICION)
        {
            Composicion(h);
            return;
        }

        for (const auto &c : h.categorias)
            Err("E-STR-07", c.pos,
                "La sintaxis de categoria vehicular ('" + c.nombre +
                    " NUM %') no esta permitida fuera de 'composicion' (aparece en '" +
                    Nombre(h.tipo) + "').");

        std::map<std::string, Medida> local;
        for (const auto &p : h.props)
            EvaluarProp(p, h.tipo, local);

        if (h.tipo == Tok::FASE)
        {
            auto it = local.find("duracion");
            if (it != local.end())
                ev.fases.push_back(it->second);
            else
                ev.fasesOk = false;
        }
        else
            ev.sub[h.tipo] = local;
    }

    /* ---------- bloque de escenario (via, puente, ..., clima, simulacion) ---------- */
    void AnalizarBloque(const BloqueAST &b, Evaluado &ev)
    {
        if (b.Vacio())
            Err("E-STR-04", b.pos,
                "Bloque '" + Nombre(b.tipo) + (b.conNombre ? " \"" + b.nombre + "\"" : "") +
                    "' vacio; faltan sus propiedades o sub-bloques obligatorios.");

        for (const auto &s : b.sueltos)
            Err("E-UNI-02", s.pos,
                "Unidad o simbolo '" + s.lexema + "' inesperado: no sigue a ningun valor.");

        for (const auto &p : b.props)
            EvaluarProp(p, b.tipo, ev.p);

        for (const auto &r : b.refs)
            RevisarRef(b, r);

        std::set<std::string> fasesVistas;
        for (const auto &h : b.hijos)
            RevisarHijo(b, h, ev, fasesVistas);
    }

    static bool TieneProp(const BloqueAST &b, const std::string &n)
    {
        for (const auto &p : b.props)
            if (p.nombre == n)
                return true;
        return false;
    }

    void ChequearSemaforo(const Evaluado &ev)
    {
        if (ev.p.count("ciclo") && ev.fasesOk && !ev.fases.empty())
        {
            double suma = 0;
            for (const auto &f : ev.fases)
                suma += f.v;
            const Medida &c = ev.p.at("ciclo");
            if (std::fabs(c.v - suma) > 1e-9)
                Err("E-DOM-04", c.pos,
                    "El 'ciclo' del semaforo (" + Fmt(c.v) + " s) no coincide con la suma de 'duracion' de sus fases (" +
                        Fmt(suma) + " s).");
        }
    }

    /* ---------- escenario ---------- */
    void AnalizarEscenario(const EscenarioAST &esc)
    {
        viasDecl.clear();
        intersDecl.clear();
        anidados.clear();

        if (esc.nombre.empty())
            Err("E-ESC-01", esc.posNombre, "Identificador de 'escenario' vacio.");

        // mapa
        if (esc.mapas.empty())
            Err("E-STR-01", esc.pos,
                esc.bloques.empty() ? "Escenario vacio; falta la propiedad obligatoria 'mapa'."
                                    : "Falta la propiedad obligatoria 'mapa' en el escenario.");
        for (std::size_t k = 0; k < esc.mapas.size(); k++)
        {
            if (esc.mapas[k].nombre.empty())
                Err("E-STR-03", esc.mapas[k].pos, "Nombre de mapa vacio.");
            if (k > 0)
                Err("E-STR-02", esc.mapas[k].pos,
                    "Propiedad 'mapa' duplicada dentro del mismo escenario.");
        }

        // pasada 1: declaraciones (permite referencias hacia adelante)
        std::set<std::pair<Tok, std::string>> vistos;
        int nClima = 0, nSim = 0;
        auto Registrar = [&](const BloqueAST &b)
        {
            if (b.nombre.empty())
            {
                Err("E-REF-02", b.posNombre,
                    "Identificador vacio en la declaracion de '" + Nombre(b.tipo) + "'.");
            }
            else if (!vistos.insert({b.tipo, b.nombre}).second)
            {
                Err("E-REF-01", b.posNombre,
                    "Identificador \"" + b.nombre + "\" duplicado para entidades '" +
                        Nombre(b.tipo) + "' dentro del escenario.");
            }
            else if (b.tipo == Tok::VIA)
                viasDecl.insert(b.nombre);
            else if (b.tipo == Tok::INTERSECCION)
                intersDecl.insert(b.nombre);
        };
        for (const auto &b : esc.bloques)
        {
            switch (b.tipo)
            {
            case Tok::CLIMA:
                if (++nClima > 1)
                    Err("E-STR-02", b.pos, "Bloque unico 'clima' duplicado en el escenario.");
                break;
            case Tok::SIMULACION:
                if (++nSim > 1)
                    Err("E-STR-02", b.pos, "Bloque unico 'simulacion' duplicado en el escenario.");
                break;
            default:
                Registrar(b);
                if (b.tipo == Tok::INTERSECCION)
                    for (const auto &h : b.hijos)
                        if (h.tipo == Tok::SEMAFORO)
                            Registrar(h); // semaforo anidado
            }
        }

        // pasada 2: reglas locales de cada bloque
        std::vector<std::pair<const BloqueAST *, Evaluado>> evals;
        for (const auto &b : esc.bloques)
        {
            Evaluado ev;
            AnalizarBloque(b, ev);
            evals.push_back({&b, std::move(ev)});
        }

        // pasada 3: reglas cruzadas
        std::map<std::string, double> carrilesVia;
        for (const auto &[b, ev] : evals)
            if (b->tipo == Tok::VIA && b->conNombre && ev.p.count("carriles") &&
                !carrilesVia.count(b->nombre))
                carrilesVia[b->nombre] = ev.p.at("carriles").v;

        const Medida *simDur = nullptr;
        for (const auto &[b, ev] : evals)
            if (b->tipo == Tok::SIMULACION && ev.p.count("duracion"))
            {
                simDur = &ev.p.at("duracion");
                break;
            }

        for (const auto &[b, ev] : evals)
        {
            switch (b->tipo)
            {
            case Tok::SEMAFORO:
                ChequearSemaforo(ev);
                break;

            case Tok::PUENTE:
            {
                auto g = ev.sub.find(Tok::GEOMETRIA);
                auto e = ev.sub.find(Tok::ESTRUCTURA);
                if (g != ev.sub.end() && e != ev.sub.end() && g->second.count("altura") &&
                    e->second.count("tablero"))
                {
                    const Medida &alt = g->second.at("altura");
                    const Medida &tab = e->second.at("tablero");
                    if (tab.v >= alt.v)
                        Err("E-DOM-10", tab.pos,
                            "Inconsistencia geometrica: el espesor del 'tablero' (" + Fmt(tab.v) +
                                " m) es mayor o igual a la 'altura' del puente (" + Fmt(alt.v) + " m).");
                }
                break;
            }

            case Tok::EVENTO:
                if (ev.p.count("carril"))
                {
                    for (const auto &r : b->refs)
                    {
                        if (r.tok != Tok::VIA)
                            continue;
                        auto it = carrilesVia.find(r.nombre);
                        const Medida &c = ev.p.at("carril");
                        if (it != carrilesVia.end() && c.v > it->second)
                            Err("E-DOM-09", c.pos,
                                "El numero de 'carril' (" + Fmt(c.v) + ") excede los 'carriles' declarados en la via \"" +
                                    r.nombre + "\" (" + Fmt(it->second) + ").");
                        break; // solo la primera via referenciada
                    }
                }
                // sigue en DEMANDA (desborde temporal)
                [[fallthrough]];
            case Tok::DEMANDA:
                if (simDur && ev.p.count("duracion"))
                {
                    bool inicioInvalido = TieneProp(*b, "inicio") && !ev.p.count("inicio");
                    if (!inicioInvalido)
                    {
                        double ini = ev.p.count("inicio") ? Seg(ev.p.at("inicio")) : 0.0;
                        const Medida &d = ev.p.at("duracion");
                        if (ini + Seg(d) > Seg(*simDur) + 1e-9)
                            Err("E-DOM-11", d.pos,
                                "Desbordamiento temporal: inicio + duracion (" +
                                    Fmt((ini + Seg(d)) / 60.0) + " min) supera la 'duracion' de la simulacion (" +
                                    Fmt(Seg(*simDur) / 60.0) + " min).");
                    }
                }
                break;

            case Tok::SIMULACION:
                if (ev.p.count("duracion") && ev.p.count("intervalo"))
                {
                    const Medida &d = ev.p.at("duracion");
                    const Medida &iv = ev.p.at("intervalo");
                    if (Seg(iv) > Seg(d))
                        Err("E-DOM-11", iv.pos,
                            "Inconsistencia temporal: 'intervalo' (" + Fmt(Seg(iv)) +
                                " s) es mayor que la 'duracion' de la simulacion (" + Fmt(Seg(d)) + " s).");
                }
                break;

            default:
                break;
            }
        }

        for (const auto &[b, ev] : anidados)
            ChequearSemaforo(ev);
    }

public:
    explicit AnalizadorSemantico(const TablaLexica &t) : tabla(t) {}

    const std::vector<ErrorSemantico> &Analizar(const ProgramaAST &prog)
    {
        errores.clear();
        vars.clear();

        for (const auto &v : prog.vars)
        {
            if (vars.count(v.nombre))
                Err("E-VAR-01", v.pos,
                    "Variable global '" + v.nombre + "' redeclarada en el nivel superior.");
            else
                vars[v.nombre] = v;
        }

        if (prog.escenarios.empty())
            Err("E-ESC-02", prog.posFin,
                "Ausencia de punto de entrada 'escenario' antes de llegar a FIN.");

        for (const auto &e : prog.escenarios)
            AnalizarEscenario(e);

        std::stable_sort(errores.begin(), errores.end(),
                         [](const ErrorSemantico &a, const ErrorSemantico &b)
                         { return a.linea != b.linea ? a.linea < b.linea : a.columna < b.columna; });
        return errores;
    }
};

/* ============================================================
 * GENERADOR DE JSON (formato intermedio, version "1.0")
 * ------------------------------------------------------------
 * Se ejecuta SOLO sobre programas sin errores semanticos.
 * Normalizacion de valores:
 *   - las variables 'var' se reemplazan por su valor numerico
 *   - tiempos en segundos (claves *_s); 'min' -> x60
 *   - longitudes en metros (*_m), velocidades en km/h (*_kmh),
 *     flujo/capacidad en veh/h (*_veh_h), angulos en grados,
 *     porcentajes en 0..100 (*_pct)
 *   - auto/autos -> "autos", bus/buses -> "buses"
 *   - los semaforos (anidados o no) se listan en "semaforos"
 *     con el id de su interseccion
 * ============================================================ */
class Json
{
public:
    enum class T { NUL, NUM, STR, ARR, OBJ };
    T t = T::NUL;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::string> claves;
    std::vector<Json> vals;

    static Json Num(double v) { Json j; j.t = T::NUM; j.n = v; return j; }
    static Json Str(const std::string &v) { Json j; j.t = T::STR; j.s = v; return j; }
    static Json Arr() { Json j; j.t = T::ARR; return j; }
    static Json Obj() { Json j; j.t = T::OBJ; return j; }

    Json &Add(Json v) { a.push_back(std::move(v)); return *this; }
    Json &Set(const std::string &k, Json v)
    {
        claves.push_back(k);
        vals.push_back(std::move(v));
        return *this;
    }

    static std::string Esc(const std::string &x)
    {
        std::string r;
        for (unsigned char c : x)
        {
            switch (c)
            {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\t': r += "\\t"; break;
            case '\r': r += "\\r"; break;
            default:
                if (c < 0x20)
                {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    r += b;
                }
                else
                    r += static_cast<char>(c);
            }
        }
        return r;
    }

    static std::string Numero(double v)
    {
        char b[48];
        if (v == std::floor(v) && std::fabs(v) < 1e15)
            std::snprintf(b, sizeof b, "%lld", static_cast<long long>(v));
        else
            std::snprintf(b, sizeof b, "%.10g", v);
        return b;
    }

    void Escribir(std::ostream &o, int nivel = 0) const
    {
        std::string ind(static_cast<std::size_t>(nivel) * 2, ' ');
        std::string ind2(static_cast<std::size_t>(nivel + 1) * 2, ' ');
        switch (t)
        {
        case T::NUL: o << "null"; break;
        case T::NUM: o << Numero(n); break;
        case T::STR: o << '"' << Esc(s) << '"'; break;
        case T::ARR:
        {
            if (a.empty()) { o << "[]"; break; }
            bool plano = true;
            for (const auto &e : a)
                if (e.t != T::NUM) plano = false;
            if (plano)
            {
                o << '[';
                for (std::size_t i = 0; i < a.size(); i++)
                    o << (i ? ", " : "") << Numero(a[i].n);
                o << ']';
                break;
            }
            o << "[\n";
            for (std::size_t i = 0; i < a.size(); i++)
            {
                o << ind2;
                a[i].Escribir(o, nivel + 1);
                o << (i + 1 < a.size() ? ",\n" : "\n");
            }
            o << ind << ']';
            break;
        }
        case T::OBJ:
        {
            if (claves.empty()) { o << "{}"; break; }
            o << "{\n";
            for (std::size_t i = 0; i < claves.size(); i++)
            {
                o << ind2 << '"' << Esc(claves[i]) << "\": ";
                vals[i].Escribir(o, nivel + 1);
                o << (i + 1 < claves.size() ? ",\n" : "\n");
            }
            o << ind << '}';
            break;
        }
        }
    }
};

class GeneradorJSON
{
private:
    const ProgramaAST &prog;
    std::map<std::string, double> vars;

    static const PropAST *Prop(const BloqueAST &b, const std::string &n)
    {
        for (const auto &p : b.props)
            if (p.nombre == n)
                return &p;
        return nullptr;
    }

    static const BloqueAST *Hijo(const BloqueAST &b, Tok t)
    {
        for (const auto &h : b.hijos)
            if (h.tipo == t)
                return &h;
        return nullptr;
    }

    double Valor(const PropAST &p) const
    {
        if (p.op.tipo == Operando::Tipo::ID)
        {
            auto it = vars.find(p.op.texto);
            return it == vars.end() ? 0.0 : it->second;
        }
        return p.op.num;
    }

    double ValorSeg(const PropAST &p) const
    {
        double v = Valor(p);
        if (!p.op.unidades.empty())
        {
            if (p.op.unidades[0] == "min") v *= 60.0;
            else if (p.op.unidades[0] == "h") v *= 3600.0;
        }
        return v;
    }

    void Num(Json &o, const BloqueAST &b, const char *prop, const char *clave, bool tiempo = false) const
    {
        if (auto p = Prop(b, prop))
            o.Set(clave, Json::Num(tiempo ? ValorSeg(*p) : Valor(*p)));
    }

    void Texto(Json &o, const BloqueAST &b, const char *prop, const char *clave) const
    {
        if (auto p = Prop(b, prop))
            o.Set(clave, Json::Str(p->op.texto));
    }

    static Json Coord(const std::vector<double> &c)
    {
        Json r = Json::Arr();
        for (double v : c)
            r.Add(Json::Num(v));
        return r;
    }

    static void Posicion(Json &o, const BloqueAST &b)
    {
        for (const auto &p : b.props)
            if (p.nombre == "posicion" && !p.op.tuplas.empty())
                o.Set("posicion", Coord(p.op.tuplas[0]));
    }

    Json Semaforo(const BloqueAST &b, const std::string &interseccion) const
    {
        Json o = Json::Obj();
        o.Set("id", Json::Str(b.nombre));
        o.Set("interseccion", interseccion.empty() ? Json() : Json::Str(interseccion));
        Num(o, b, "ciclo", "ciclo_s", true);
        Json fases = Json::Arr();
        for (const auto &h : b.hijos)
            if (h.tipo == Tok::FASE)
            {
                Json f = Json::Obj();
                f.Set("nombre", Json::Str(h.nombre));
                Num(f, h, "duracion", "duracion_s", true);
                fases.Add(f);
            }
        o.Set("fases", fases);
        return o;
    }

    Json Escenario(const EscenarioAST &esc) const
    {
        Json e = Json::Obj();
        e.Set("nombre", Json::Str(esc.nombre));
        e.Set("mapa", esc.mapas.empty() ? Json() : Json::Str(esc.mapas[0].nombre));

        Json vias = Json::Arr(), inter = Json::Arr(), sem = Json::Arr(), puentes = Json::Arr(),
             edificios = Json::Arr(), demandas = Json::Arr(), eventos = Json::Arr();
        const BloqueAST *clima = nullptr, *simulacion = nullptr;

        for (const auto &b : esc.bloques)
        {
            switch (b.tipo)
            {
            case Tok::VIA:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                Texto(o, b, "tipo", "tipo");
                if (auto p = Prop(b, "puntos"))
                {
                    Json pts = Json::Arr();
                    for (const auto &c : p->op.tuplas)
                        pts.Add(Coord(c));
                    o.Set("puntos", pts);
                }
                Num(o, b, "carriles", "carriles");
                Num(o, b, "ancho_carril", "ancho_carril_m");
                Texto(o, b, "sentido", "sentido");
                Num(o, b, "velocidad_max", "velocidad_max_kmh");
                Num(o, b, "capacidad", "capacidad_veh_h");
                vias.Add(o);
                break;
            }
            case Tok::INTERSECCION:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                Posicion(o, b);
                Json con = Json::Arr();
                for (const auto &p : b.props)
                    if (p.nombre == "conecta")
                        con.Add(Json::Str(p.op.texto));
                o.Set("conecta", con);
                inter.Add(o);
                for (const auto &h : b.hijos)
                    if (h.tipo == Tok::SEMAFORO)
                        sem.Add(Semaforo(h, b.nombre));
                break;
            }
            case Tok::SEMAFORO:
            {
                std::string ref;
                for (const auto &r : b.refs)
                    if (r.tok == Tok::INTERSECCION)
                    {
                        ref = r.nombre;
                        break;
                    }
                sem.Add(Semaforo(b, ref));
                break;
            }
            case Tok::PUENTE:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                Texto(o, b, "tipo", "tipo");
                Posicion(o, b);
                Num(o, b, "orientacion", "orientacion_grados");
                if (auto g = Hijo(b, Tok::GEOMETRIA))
                {
                    Json x = Json::Obj();
                    Num(x, *g, "longitud", "longitud_m");
                    Num(x, *g, "ancho", "ancho_m");
                    Num(x, *g, "altura", "altura_m");
                    o.Set("geometria", x);
                }
                if (auto c = Hijo(b, Tok::CALZADA))
                {
                    Json x = Json::Obj();
                    Num(x, *c, "carriles", "carriles");
                    Num(x, *c, "ancho_carril", "ancho_carril_m");
                    Texto(x, *c, "sentido", "sentido");
                    Num(x, *c, "velocidad_max", "velocidad_max_kmh");
                    Num(x, *c, "capacidad", "capacidad_veh_h");
                    o.Set("calzada", x);
                }
                if (auto s = Hijo(b, Tok::ESTRUCTURA))
                {
                    Json x = Json::Obj();
                    Num(x, *s, "tablero", "tablero_m");
                    Num(x, *s, "apoyo", "apoyos");
                    o.Set("estructura", x);
                }
                if (auto p = Prop(b, "sobre"))
                    o.Set("sobre", Json::Str(p->op.texto));
                puentes.Add(o);
                break;
            }
            case Tok::EDIFICIO:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                Posicion(o, b);
                if (auto d = Hijo(b, Tok::DIMENSIONES))
                {
                    Json x = Json::Obj();
                    Num(x, *d, "largo", "largo_m");
                    Num(x, *d, "ancho", "ancho_m");
                    Num(x, *d, "altura", "altura_m");
                    o.Set("dimensiones", x);
                }
                Num(o, b, "rotacion", "rotacion_grados");
                edificios.Add(o);
                break;
            }
            case Tok::DEMANDA:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                for (const auto &r : b.refs)
                    if (r.tok == Tok::VIA)
                    {
                        o.Set("via", Json::Str(r.nombre));
                        break;
                    }
                Num(o, b, "flujo", "flujo_veh_h");
                if (auto c = Hijo(b, Tok::COMPOSICION))
                {
                    std::map<std::string, double> m;
                    std::vector<std::string> orden;
                    for (const auto &k : c->categorias)
                    {
                        std::string n = k.nombre == "auto" ? "autos" : k.nombre == "bus" ? "buses" : k.nombre;
                        if (!m.count(n))
                            orden.push_back(n);
                        m[n] += k.num;
                    }
                    Json x = Json::Obj();
                    for (const auto &n : orden)
                        x.Set(n, Json::Num(m[n]));
                    o.Set("composicion_pct", x);
                }
                Num(o, b, "inicio", "inicio_s", true);
                Num(o, b, "duracion", "duracion_s", true);
                demandas.Add(o);
                break;
            }
            case Tok::EVENTO:
            {
                Json o = Json::Obj();
                o.Set("id", Json::Str(b.nombre));
                Texto(o, b, "tipo", "tipo");
                for (const auto &r : b.refs)
                    if (r.tok == Tok::VIA)
                    {
                        o.Set("via", Json::Str(r.nombre));
                        break;
                    }
                Num(o, b, "carril", "carril");
                Num(o, b, "inicio", "inicio_s", true);
                Num(o, b, "duracion", "duracion_s", true);
                eventos.Add(o);
                break;
            }
            case Tok::CLIMA:
                if (!clima) clima = &b;
                break;
            case Tok::SIMULACION:
                if (!simulacion) simulacion = &b;
                break;
            default:
                break;
            }
        }

        e.Set("vias", vias);
        e.Set("intersecciones", inter);
        e.Set("semaforos", sem);
        e.Set("puentes", puentes);
        e.Set("edificios", edificios);
        e.Set("demandas", demandas);
        e.Set("eventos", eventos);

        if (clima)
        {
            Json o = Json::Obj();
            Texto(o, *clima, "estado", "estado");
            Texto(o, *clima, "intensidad", "intensidad");
            Num(o, *clima, "visibilidad", "visibilidad_pct");
            e.Set("clima", o);
        }
        else
            e.Set("clima", Json());

        if (simulacion)
        {
            Json o = Json::Obj();
            Num(o, *simulacion, "duracion", "duracion_s", true);
            Num(o, *simulacion, "intervalo", "intervalo_s", true);
            Num(o, *simulacion, "repeticiones", "repeticiones");
            e.Set("simulacion", o);
        }
        else
            e.Set("simulacion", Json());
        return e;
    }

public:
    explicit GeneradorJSON(const ProgramaAST &p) : prog(p)
    {
        for (const auto &v : p.vars)
            if (!vars.count(v.nombre))
                vars[v.nombre] = v.valor;
    }

    std::string Generar() const
    {
        Json raiz = Json::Obj();
        raiz.Set("version", Json::Str("1.0"));
        Json vs = Json::Obj();
        for (const auto &v : prog.vars)
            vs.Set(v.nombre, Json::Num(v.valor));
        raiz.Set("variables", vs);
        // Un archivo .usim tiene un escenario; se emite el primero
        raiz.Set("escenario", prog.escenarios.empty() ? Json() : Escenario(prog.escenarios[0]));
        std::ostringstream o;
        raiz.Escribir(o);
        o << "\n";
        return o.str();
    }
};

/* ============================================================
 * PIPELINE COMPLETO: lexico -> sintactico -> semantico
 * ============================================================ */
struct ResultadoAnalisis
{
    bool sintacticoOk = false;
    std::optional<ErrorSintactico> errorSintactico;
    std::vector<ErrorSemantico> errores;
    std::optional<ProgramaAST> programa; // AST (solo si el sintactico acepto)
};

static ResultadoAnalisis AnalizarCodigo(const std::string &codigo)
{
    ResultadoAnalisis res;
    TablaLexica tabla;

    {
        Tokenizador tk(codigo, tabla);
        AnalizadorSintactico parser(tk, false);
        res.sintacticoOk = parser.Analizar();
        if (!res.sintacticoOk)
        {
            res.errorSintactico = parser.getError();
            return res; // el semantico exige un programa sintacticamente valido
        }
    }

    std::vector<Token> tokens;
    {
        Tokenizador tk(codigo, tabla);
        while (true)
        {
            Token t = tk.getToken();
            tokens.push_back(t);
            if (t.token == Tok::FIN)
                break;
        }
    }

    ProgramaAST prog = ConstructorAST(std::move(tokens)).Construir();
    AnalizadorSemantico sem(tabla);
    res.errores = sem.Analizar(prog);
    res.programa = std::move(prog);
    return res;
}

static void ImprimirResultado(const std::string &titulo, const ResultadoAnalisis &r)
{
    std::cout << "\n############################################################\n";
    std::cout << "PRUEBA: " << titulo << "\n";
    std::cout << "############################################################\n";

    if (!r.sintacticoOk)
    {
        std::cout << "Fase sintactica: RECHAZADO\n"
                  << r.errorSintactico->mensaje << "\n"
                  << "(No se ejecuta el analisis semantico.)\n";
        return;
    }

    std::cout << "Fase sintactica: ACEPTADO\n";
    if (r.errores.empty())
    {
        std::cout << "Fase semantica : OK (0 errores)\n";
        return;
    }

    std::cout << "Fase semantica : " << r.errores.size() << " error(es)\n";
    for (const auto &e : r.errores)
        std::cout << "  [" << e.codigo << "] linea " << e.linea << ", columna " << e.columna
                  << ": " << e.mensaje << "\n";
}

/* ============================================================
 * SUITE DE PRUEBAS: un caso por cada error del catalogo.
 * Cada caso debe producir EXACTAMENTE los codigos esperados
 * (asi se detectan tambien errores en cascada).
 * ============================================================ */
struct Caso
{
    std::string nombre;
    std::string codigo;
    std::set<std::string> esperados;
};

static std::string Esc(const std::string &cuerpo)
{
    return "escenario \"T\" {\n mapa \"M\"\n" + cuerpo + "\n}\n";
}

static const char *VIA_V = " via \"V\" { carriles 2 }\n";

static std::vector<Caso> CrearCasos()
{
    std::vector<Caso> c;
    auto add = [&](const std::string &n, const std::string &cod, std::set<std::string> e)
    { c.push_back({n, cod, std::move(e)}); };

    // 1. Nivel superior
    add("E-VAR-01 variable redeclarada", "var x = 10\nvar x = 20\n" + Esc(""), {"E-VAR-01"});
    add("E-ESC-01 escenario con nombre vacio", "escenario \"\" {\n mapa \"M\"\n}\n", {"E-ESC-01"});
    add("E-ESC-02 sin escenario", "var x = 10\n", {"E-ESC-02"});

    // 2. Elementos de escenario
    add("E-STR-01 escenario vacio", "escenario \"UNA\" { }\n", {"E-STR-01"});
    add("E-STR-02 mapa duplicado", Esc("mapa \"M2\""), {"E-STR-02"});
    add("E-STR-03 mapa vacio", "escenario \"T\" {\n mapa \"\"\n}\n", {"E-STR-03"});
    add("E-STR-02 clima duplicado", Esc("clima { estado lluvia }\n clima { estado lluvia }"), {"E-STR-02"});
    add("E-STR-02 simulacion duplicada", Esc("simulacion { repeticiones 1 }\n simulacion { repeticiones 1 }"), {"E-STR-02"});
    add("E-REF-01 via duplicada", Esc("via \"A\" { carriles 2 }\n via \"A\" { carriles 2 }"), {"E-REF-01"});
    add("E-REF-01 edificio duplicado", Esc("edificio \"E\" { rotacion 10 grados }\n edificio \"E\" { rotacion 10 grados }"), {"E-REF-01"});
    add("E-REF-02 via con id vacio", Esc("via \"\" { carriles 2 }"), {"E-REF-02"});
    add("E-REF-02 semaforo con id vacio", Esc("semaforo \"\" { ciclo 60 s }"), {"E-REF-02"});

    // 3. Bloques de segundo nivel
    add("E-STR-04 via vacia", Esc("via \"V1\" { }"), {"E-STR-04"});
    add("E-STR-04 edificio vacio", Esc("edificio \"E1\" { }"), {"E-STR-04"});
    add("E-STR-04 interseccion vacia", Esc("interseccion \"I1\" { }"), {"E-STR-04"});
    add("E-STR-04 semaforo vacio", Esc("semaforo \"S1\" { }"), {"E-STR-04"});
    add("E-STR-05 via dentro de via", Esc("via \"V1\" { via \"V2\" }"), {"E-STR-05"});
    add("E-STR-05 via dentro de edificio", Esc("edificio \"E1\" { via \"V2\" }"), {"E-STR-05"});
    add("E-STR-05 interseccion dentro de via", Esc("via \"V1\" { interseccion \"I1\" }"), {"E-STR-05"});
    add("E-STR-05 interseccion dentro de puente", Esc("puente \"P1\" { interseccion \"I1\" }"), {"E-STR-05"});
    add("E-STR-05 fase dentro de edificio", Esc("edificio \"E1\" { fase verde { duracion 10 s } }"), {"E-STR-05"});
    add("E-STR-06 composicion en via", Esc("via \"V1\" { composicion { autos 100 % } }"), {"E-STR-06"});
    add("E-STR-06 estructura en semaforo", Esc("semaforo \"S1\" { estructura { tablero 1 m } }"), {"E-STR-06"});
    add("E-STR-06 geometria en demanda", Esc("demanda \"D1\" { geometria { altura 1 m } }"), {"E-STR-06"});
    add("E-STR-06 dimensiones en clima", Esc("clima { dimensiones { largo 1 m } }"), {"E-STR-06"});
    add("E-REF-03 demanda -> via inexistente", Esc("demanda \"D1\" { via \"Av_Inexistente\" }"), {"E-REF-03"});
    add("E-REF-03 evento -> via inexistente", Esc("evento \"E1\" { via \"Av_Inexistente\" }"), {"E-REF-03"});
    add("E-REF-03 interseccion conecta via inexistente", Esc("interseccion \"I1\" { conecta \"Av_Inexistente\" }"), {"E-REF-03"});
    add("E-REF-03 puente sobre via inexistente", Esc("puente \"P1\" { sobre \"Av_Inexistente\" }"), {"E-REF-03"});
    add("E-REF-03 semaforo -> interseccion inexistente", Esc("semaforo \"S1\" { interseccion \"Cruce_Inexistente\" }"), {"E-REF-03"});

    // 4. Sub-bloques y fases
    add("E-DOM-01 fase azul", Esc("semaforo \"S1\" { fase azul { duracion 10 s } }"), {"E-DOM-01"});
    add("E-DOM-02 fase duplicada", Esc("semaforo \"S1\" { fase verde { duracion 10 s } fase verde { duracion 10 s } }"), {"E-DOM-02"});
    add("E-STR-07 categoria en dimensiones", Esc("edificio \"E1\" { dimensiones { autos 60 % } }"), {"E-STR-07"});
    add("E-STR-07 categoria en geometria", Esc("puente \"P1\" { geometria { autos 60 % } }"), {"E-STR-07"});
    add("E-STR-07 categoria en fase", Esc("semaforo \"S1\" { fase verde { autos 60 % } }"), {"E-STR-07"});
    add("E-STR-07 dimension en composicion", Esc("demanda \"D1\" { composicion { largo 40 m } }"), {"E-STR-07"});
    add("E-UNI-01 composicion con 'm'", Esc("demanda \"D1\" { composicion { autos 60 m } }"), {"E-UNI-01"});
    add("E-UNI-01 composicion sin '%'", Esc("demanda \"D1\" { composicion { autos 60 } }"), {"E-UNI-01"});
    add("E-DOM-03 suma != 100", Esc("demanda \"D1\" { composicion { autos 60 % buses 15 % motos 15 % camiones 5 % } }"), {"E-DOM-03"});
    add("E-DOM-04 ciclo != suma fases", Esc("semaforo \"S1\" { ciclo 65 s fase verde { duracion 30 s } fase amarillo { duracion 5 s } fase rojo { duracion 20 s } }"), {"E-DOM-04"});

    // 5. Propiedades genericas y unidades
    add("E-VAR-02 variable no declarada", Esc("via \"V\" { velocidad_max variable_no_declarada kmh }"), {"E-VAR-02"});
    add("E-TIP-01 var decimal en entero", "var var_global = 2.5\n" + Esc("via \"V\" { carriles var_global }"), {"E-TIP-01"});
    add("E-TIP-02 carriles \"dos\"", Esc("via \"V\" { carriles \"dos\" }"), {"E-TIP-02"});
    add("E-TIP-02 tipo \"avenida\"", Esc("via \"V\" { tipo \"avenida\" }"), {"E-TIP-02"});
    add("E-TIP-02 ancho_carril tupla", Esc("via \"V\" { ancho_carril (1, 2, 3) }"), {"E-TIP-02"});
    add("E-TIP-02 velocidad_max lista", Esc("via \"V\" { velocidad_max [ (1, 2, 3) ] }"), {"E-TIP-02"});
    add("E-TIP-03 carriles 2.5", Esc("via \"V\" { carriles 2.5 }"), {"E-TIP-03"});
    add("E-TIP-03 apoyo 1.5", Esc("puente \"P\" { estructura { apoyo 1.5 } }"), {"E-TIP-03"});
    add("E-TIP-03 repeticiones 3.2", Esc("simulacion { repeticiones 3.2 }"), {"E-TIP-03"});
    add("E-DOM-05 tipo peatonal", Esc("via \"V\" { tipo peatonal }"), {"E-DOM-05"});
    add("E-DOM-05 sentido norte", Esc("via \"V\" { sentido norte }"), {"E-DOM-05"});
    add("E-DOM-05 estado nevado", Esc("clima { estado nevado }"), {"E-DOM-05"});
    add("E-DOM-05 intensidad extrema", Esc("clima { intensidad extrema }"), {"E-DOM-05"});
    add("E-UNI-02 carriles 2 m", Esc("via \"V\" { carriles 2 m }"), {"E-UNI-02"});
    add("E-UNI-02 tipo avenida kmh", Esc("via \"V\" { tipo avenida kmh }"), {"E-UNI-02"});
    add("E-UNI-02 conecta \"V\" %", Esc(std::string(VIA_V) + "interseccion \"I\" { conecta \"V\" % }"), {"E-UNI-02"});
    add("E-UNI-03 ancho_carril sin unidad", Esc("via \"V\" { ancho_carril 3.5 }"), {"E-UNI-03"});
    add("E-UNI-03 velocidad_max sin unidad", Esc("via \"V\" { velocidad_max 40 }"), {"E-UNI-03"});
    add("E-UNI-03 ciclo sin unidad", Esc("semaforo \"S\" { ciclo 90 }"), {"E-UNI-03"});
    add("E-UNI-03 flujo sin unidad", Esc("demanda \"D\" { flujo 900 }"), {"E-UNI-03"});
    add("E-UNI-03 visibilidad sin unidad", Esc("clima { visibilidad 70 }"), {"E-UNI-03"});
    add("E-UNI-04 ancho_carril kmh", Esc("via \"V\" { ancho_carril 3.5 kmh }"), {"E-UNI-04"});
    add("E-UNI-04 velocidad_max m", Esc("via \"V\" { velocidad_max 40 m }"), {"E-UNI-04"});
    add("E-UNI-04 ciclo min", Esc("semaforo \"S\" { ciclo 90 min }"), {"E-UNI-04"});
    add("E-UNI-04 duracion s (simulacion)", Esc("simulacion { duracion 60 s }"), {"E-UNI-04"});
    add("E-UNI-04 duracion s (demanda)", Esc("demanda \"D\" { duracion 60 s }"), {"E-UNI-04"});
    add("E-UNI-04 duracion s (evento)", Esc("evento \"E\" { duracion 60 s }"), {"E-UNI-04"});

    // 6. Listas y tuplas
    add("E-TIP-04 tupla de 2", Esc("interseccion \"I\" { posicion (500, 0) }"), {"E-TIP-04"});
    add("E-TIP-04 tupla de 4", Esc("interseccion \"I\" { posicion (500, 0, 80, 10) }"), {"E-TIP-04"});
    add("E-DOM-06 puntos con 1 coordenada", Esc("via \"V\" { puntos [ (100, 0, 50) ] }"), {"E-DOM-06"});
    add("E-DOM-07 puntos consecutivos iguales", Esc("via \"V\" { puntos [ (100, 0, 50), (100, 0, 50) ] }"), {"E-DOM-07"});

    // 7. Rango y consistencia fisica
    add("E-DOM-08 carriles 0", Esc("via \"V\" { carriles 0 }"), {"E-DOM-08"});
    add("E-DOM-08 ancho_carril -3.5 m", Esc("via \"V\" { ancho_carril -3.5 m }"), {"E-DOM-08"});
    add("E-DOM-08 velocidad_max 0 kmh", Esc("via \"V\" { velocidad_max 0 kmh }"), {"E-DOM-08"});
    add("E-DOM-08 largo 0 m", Esc("edificio \"E\" { dimensiones { largo 0 m } }"), {"E-DOM-08"});
    add("E-DOM-08 duracion -5 min", Esc("simulacion { duracion -5 min }"), {"E-DOM-08"});
    add("E-DOM-08 visibilidad 120 %", Esc("clima { visibilidad 120 % }"), {"E-DOM-08"});
    add("E-DOM-08 orientacion 400 grados", Esc("puente \"P\" { orientacion 400 grados }"), {"E-DOM-08"});
    add("E-DOM-09 carril 3 en via de 2", Esc(std::string(VIA_V) + "evento \"Acc_1\" { via \"V\" carril 3 }"), {"E-DOM-09"});
    add("E-DOM-10 tablero >= altura", Esc("puente \"P1\" { geometria { altura 5 m } estructura { tablero 5.5 m } }"), {"E-DOM-10"});
    add("E-DOM-11 inicio+duracion > simulacion",
        Esc("simulacion { duracion 60 min }\n demanda \"D\" { inicio 50 min duracion 20 min }"), {"E-DOM-11"});
    add("E-DOM-11 intervalo > duracion", Esc("simulacion { duracion 1 min intervalo 120 s }"), {"E-DOM-11"});

    // Extensiones para los ejercicios 3, 9, 15 y 16
    add("OK tuplas de una lista sin coma (ej. 3)", Esc("via \"V\" { puntos [ (1, 0, 1) (2, 0, 1) (3, 0, 2) ] }"), {});
    add("OK semaforo anidado en interseccion (ej. 9)", Esc("interseccion \"I\" { posicion (1, 0, 1) semaforo \"S\" { ciclo 90 s } }"), {});
    add("E-STR-05 semaforo anidado en via", Esc("via \"V\" { semaforo \"S\" { ciclo 90 s } }"), {"E-STR-05"});
    add("E-DOM-04 semaforo anidado con ciclo != fases",
        Esc("interseccion \"I\" { semaforo \"S\" { ciclo 90 s fase verde { duracion 30 s } } }"), {"E-DOM-04"});
    add("E-REF-01 semaforo anidado duplicado",
        Esc("interseccion \"I\" { semaforo \"S\" { ciclo 90 s } }\n semaforo \"S\" { ciclo 90 s }"), {"E-REF-01"});
    add("OK calzada en puente (ej. 15)", Esc(std::string(VIA_V) + "puente \"P\" { sobre \"V\" calzada { carriles 2 ancho_carril 4 m sentido doble velocidad_max 40 kmh } }"), {});
    add("E-STR-06 calzada en via", Esc("via \"V\" { calzada { carriles 2 } }"), {"E-STR-06"});
    add("E-DOM-08 calzada con carriles 0", Esc("puente \"P\" { calzada { carriles 0 } }"), {"E-DOM-08"});

    return c;
}

static const char *PROGRAMA_VALIDO = R"URB(
// Programa semanticamente valido
var v_avenida = 40

escenario "Simulacion_UNA" {
    mapa "UNA_Puno_3D"

    via "Av_Universitaria" {
        tipo avenida
        puntos [
            (100, 0, 200),
            (250, 0, 200),
            (400, 0, 250)
        ]
        carriles 4
        ancho_carril 3.50 m
        sentido doble
        velocidad_max v_avenida kmh
        capacidad 1800 veh_h
    }

    interseccion "Interseccion_Central" {
        posicion (250, 0, 200)
        conecta "Av_Universitaria"
    }

    semaforo "Semaforo_Central" {
        interseccion "Interseccion_Central"
        fase verde { duracion 30 s }
        fase rojo { duracion 30 s }
        ciclo 60 s
    }

    puente "Puente_Floral" {
        tipo vehicular
        posicion (520, 6, 80)
        orientacion 15 grados
        geometria {
            longitud 120 m
            ancho 8 m
            altura 6 m
        }
        estructura {
            tablero 0.30 m
            apoyo 2
        }
        sobre "Av_Universitaria"
    }

    edificio "Biblioteca" {
        posicion (300, 0, 100)
        dimensiones {
            largo 20 m
            ancho 15 m
            altura 12 m
        }
        rotacion 30 grados
    }

    demanda "Hora_Punta" {
        via "Av_Universitaria"
        flujo 1000 veh_h
        composicion {
            autos 60 %
            buses 15 %
            motos 15 %
            camiones 10 %
        }
        inicio 0 min
        duracion 60 min
    }

    clima {
        estado lluvia
        intensidad media
        visibilidad 70 %
    }

    evento "Accidente_01" {
        tipo accidente
        via "Av_Universitaria"
        carril 1
        inicio 20 min
        duracion 15 min
    }

    simulacion {
        duracion 60 min
        intervalo 1 s
        repeticiones 5
    }
}
)URB";

static int EjecutarSuite()
{
    int ok = 0, fallos = 0;

    // Programa valido: 0 errores en ambas fases
    {
        auto r = AnalizarCodigo(PROGRAMA_VALIDO);
        bool pasa = r.sintacticoOk && r.errores.empty();
        std::cout << (pasa ? "[ OK  ] " : "[FALLO] ") << "Programa valido completo\n";
        if (!pasa)
            ImprimirResultado("Programa valido completo", r);
        pasa ? ok++ : fallos++;
    }

    for (const auto &caso : CrearCasos())
    {
        auto r = AnalizarCodigo(caso.codigo);
        std::set<std::string> obtenidos;
        for (const auto &e : r.errores)
            obtenidos.insert(e.codigo);

        bool pasa = r.sintacticoOk && obtenidos == caso.esperados;
        std::cout << (pasa ? "[ OK  ] " : "[FALLO] ") << caso.nombre << "\n";
        if (!pasa)
        {
            ImprimirResultado(caso.nombre, r);
            std::cout << "  esperado:";
            for (const auto &e : caso.esperados)
                std::cout << " " << e;
            std::cout << "\n";
        }
        pasa ? ok++ : fallos++;
    }

    std::cout << "\nResumen: " << ok << " correctas, " << fallos << " con fallo\n";
    return fallos == 0 ? 0 : 1;
}

/* ============================================================
 * DEMOSTRACION
 * ============================================================ */

static void EjecutarPrueba(const std::string &titulo, const std::string &codigo,
                           bool mostrarTraza)
{
    TablaLexica tablaLexica;
    Tokenizador tokenizador(codigo, tablaLexica);
    AnalizadorSintactico parser(tokenizador, mostrarTraza);
    bool valido = parser.Analizar();

    std::cout << "\n############################################################\n";
    std::cout << "PRUEBA: " << titulo << "\n";
    std::cout << "############################################################\n";

    if (mostrarTraza)
    {
        std::cout << "\n"
                  << std::left << std::setw(5) << "Paso" << std::setw(10) << "Estado"
                  << std::setw(14) << "Token" << std::setw(20) << "Lexema"
                  << std::setw(8) << "Pila" << std::setw(8) << "Pila'"
                  << "Siguiente\n";
        std::cout << std::string(78, '-') << "\n";

        for (const auto &p : parser.getSeguimiento())
        {
            std::cout << std::left << std::setw(5) << p.paso
                      << std::setw(10) << NombreEst(p.estadoActual)
                      << std::setw(14) << p.clase
                      << std::setw(20) << (p.lexema ? *p.lexema : "null")
                      << std::setw(8) << p.pilaAntes
                      << std::setw(8) << p.pilaDespues
                      << (p.estadoSiguiente ? NombreEst(*p.estadoSiguiente) : "ERROR")
                      << "\n";
        }
    }

    std::cout << "\nResultado: " << (valido ? "ACEPTADO" : "RECHAZADO") << "\n";
    if (!valido)
        std::cout << parser.getError()->mensaje << "\n";
}

static void DemoSintactico()
{
    // 1) Programa valido (usa todo lo que el automata soporta)
    const std::string valido = R"URB(
// Programa valido para el automata
var v_avenida = 40

escenario "Simulacion_UNA" {
    mapa "UNA_Puno_3D"

    via "Av_Universitaria" {
        tipo avenida
        puntos [
            (100, 0, 200),
            (250, 0, 200),
            (400, 0, 250)
        ]
        carriles 4
        ancho_carril 3.50 m
        sentido doble
        velocidad_max v_avenida kmh
        capacidad 1800 veh_h
    }

    interseccion "Interseccion_Central" {
        posicion (250, 0, 200)
        conecta "Av_Universitaria"
    }

    semaforo "Semaforo_Central" {
        interseccion "Interseccion_Central"
        fase verde {
            duracion 30 s
        }
        fase rojo {
            duracion 30 s
        }
        ciclo 65 s
    }

    puente "Puente_Floral" {
        tipo vehicular
        posicion (520, 6, 80)
        orientacion 15 grados
        geometria {
            longitud 120 m
            ancho 8 m
            altura 6 m
        }
        estructura {
            tablero 0.30 m
            apoyo 2
        }
        sobre "Av_Universitaria"
    }

    edificio "Biblioteca" {
        posicion (300, 0, 100)
        dimensiones {
            largo 20 m
            ancho 15 m
            altura 12 m
        }
        rotacion 30 grados
    }

    demanda "Hora_Punta" {
        via "Av_Universitaria"
        flujo 1000 veh_h
        composicion {
            autos 60 %
            buses 15 %
            motos 15 %
            camiones 10 %
        }
        inicio 0 min
        duracion 60 min
    }

    clima {
        estado lluvia
        intensidad media
        visibilidad 70 %
    }

    evento "Accidente_01" {
        tipo accidente
        via "Av_Universitaria"
        carril 1
        inicio 20 min
        duracion 15 min
    }

    simulacion {
        duracion 60 min
        intervalo 1 s
        repeticiones 5
    }
}
)URB";

    EjecutarPrueba("Programa valido completo", valido, true);

    // 2) Programas con errores (sin traza, solo el mensaje)
    EjecutarPrueba("Falta la llave de apertura del escenario",
                   "escenario \"A\"\n mapa \"B\"\n}\n", false);

    EjecutarPrueba("Falta cerrar una llave (llega FIN con pila no vacia)",
                   "escenario \"A\" {\n via \"V\" {\n  carriles 4\n }\n", false);

    EjecutarPrueba("Tupla incompleta: falta una coma",
                   "escenario \"A\" {\n via \"V\" {\n  puntos [ (1 2, 3) ]\n }\n}\n", false);

    EjecutarPrueba("Propiedad fuera de contexto (carriles directo en escenario)",
                   "escenario \"A\" {\n carriles 4\n}\n", false);

    EjecutarPrueba("Llave de cierre cruzada con corchete",
                   "escenario \"A\" {\n via \"V\" {\n  puntos [ (1,2,3) }\n }\n}\n", false);

    EjecutarPrueba("Codigo despues del escenario",
                   "escenario \"A\" {\n}\nvia \"X\" {}\n", false);

    EjecutarPrueba("Construccion fuera del automata (function)",
                   "function f(x) { return 1 }\n", false);

    EjecutarPrueba("Error lexico (numero 12.4.5)",
                   "var x = 12.4.5\n", false);

}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        std::string a = argv[1];
        if (a == "--sintactico")
        {
            DemoSintactico();
            return 0;
        }
        if (a == "--pruebas")
            return EjecutarSuite();

        if (a == "--json")
        {
            if (argc < 3)
            {
                std::cerr << "Uso: " << argv[0] << " --json archivo.usim [salida.json]\n";
                return 2;
            }
            std::ifstream f(argv[2], std::ios::binary);
            if (!f)
            {
                std::cerr << "No se pudo abrir '" << argv[2] << "'\n";
                return 2;
            }
            std::ostringstream ss;
            ss << f.rdbuf();
            auto r = AnalizarCodigo(ss.str());
            if (!r.sintacticoOk || !r.errores.empty())
            {
                ImprimirResultado(argv[2], r);
                std::cerr << "\nNo se genera JSON: el programa tiene errores.\n";
                return 1;
            }
            std::string json = GeneradorJSON(*r.programa).Generar();
            if (argc >= 4)
            {
                std::ofstream out(argv[3], std::ios::binary);
                out << json;
                std::cerr << "JSON generado en " << argv[3] << "\n";
            }
            else
                std::cout << json;
            return 0;
        }

        // Analizar un archivo .usim
        std::ifstream f(a, std::ios::binary);
        if (!f)
        {
            std::cerr << "No se pudo abrir '" << a << "'\n";
            return 2;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        auto r = AnalizarCodigo(ss.str());
        ImprimirResultado(a, r);
        return (r.sintacticoOk && r.errores.empty()) ? 0 : 1;
    }

    // Sin argumentos: programa valido + suite del catalogo
    ImprimirResultado("Programa valido completo", AnalizarCodigo(PROGRAMA_VALIDO));
    std::cout << "\n=== SUITE DEL CATALOGO DE ERRORES SEMANTICOS ===\n";

    return EjecutarSuite();
}
