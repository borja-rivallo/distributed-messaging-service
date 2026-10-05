typedef string nombre<256>;
typedef string accion<256>;
typedef string fichero<256>;

program PROGRAMA_REGISTRO {
    version VERSION_REGISTRO {
        void registrar_accion(nombre, accion, fichero) = 1;
    } = 1;
} = 99;