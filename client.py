from enum import Enum
import argparse
import socket
import threading
import zeep

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _usuario_actual = None
    _sc_escucha = None
    _hilo_escucha = None
    _stop_event = threading.Event()

    # Usaremos un diccionario para almacenar los usuarios conectados
    _diccionario_usuarios = {} 

    @staticmethod
    def _enviar_cadena(sc, texto):
        # Función auxiliar para envía una cadena de texto terminada en \0
        sc.sendall((texto + '\0').encode('utf-8'))

    @staticmethod
    def _leer_cadena(sc):
        # Función auxiliar que recibe caracteres hasta encontrar el \0
        buffer = []
        while True:
            c = sc.recv(1).decode('utf-8', errors='ignore')
            if c == '\0' or not c:
                break
            buffer.append(c)
        return ''.join(buffer)
    
    @staticmethod
    def _leer_byte(sc):
        # Función auxiliar que recibe un único byte para leer la respuesta del servidor
        datos = sc.recv(1)
        if not datos:
            return None
        return int.from_bytes(datos, byteorder='little')

    @staticmethod
    def _tratar_mensajes():
        # Función auxiliar que es un bucle para recibir mensajes del servidor
        while not client._stop_event.is_set():
            try:
                client._sc_escucha.settimeout(1.0)
                conn, addr = client._sc_escucha.accept()
                
                op_str = client._leer_cadena(conn)
                if op_str == "SEND MESSAGE":
                    remitente = client._leer_cadena(conn)
                    id_str = client._leer_cadena(conn)
                    contenido = client._leer_cadena(conn)
                    
                    print(f"\r   \r\ns> MESSAGE {id_str} FROM {remitente}\n{contenido}\nEND")
                    print("c> ", end="", flush=True)
                    
                elif op_str == "SEND MESS_ACK":
                    id_str = client._leer_cadena(conn)
                    print(f"\r   \r\nc> SEND MESSAGE {id_str} OK")
                    print("c> ", end="", flush=True)

                # Misma lógica per con los ficheros adjuntos
                elif op_str == "SEND MESSAGE_ATTACH":
                    remitente = client._leer_cadena(conn)
                    id_str = client._leer_cadena(conn)
                    contenido = client._leer_cadena(conn)
                    fichero = client._leer_cadena(conn)
                    print(f"\r   \r\nc> MESSAGE {id_str} FROM {remitente}\n{contenido}\nEND\nFILE {fichero}")
                    print("c> ", end="", flush=True)

                elif op_str == "SEND_MESS_ATTACH_ACK":
                    id_str = client._leer_cadena(conn)
                    fichero = client._leer_cadena(conn)
                    print(f"\r   \r\nc> SENDATTACH MESSAGE {id_str} {fichero} OK")
                    print("c> ", end="", flush=True)

                # Caso para tratar solicitudes de ficheros
                elif op_str == "GET FILE":
                    # Recibimos el nombre del solicitante y el fichero que quiere
                    solicitante = client._leer_cadena(conn)
                    fichero_solicitado = client._leer_cadena(conn)
                    
                    # Hemos decidido recibir el fichero en bloques de 4096 bytes
                    try:
                        with open(fichero_solicitado, "rb") as fichero:
                            while True:
                                bytes_leidos = fichero.read(4096)
                                if not bytes_leidos:
                                    break
                                conn.sendall(bytes_leidos)
                    except Exception as e:
                        # Si ocurren errores no enviamos nada y la conexión se cerrará.
                        pass
                conn.close()
            except socket.timeout:
                continue
            except Exception:
                break

    # ******************** METHODS *******************
    # *
    # * @param user - User name to register in the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                # Se conecta al servidor y envía la petición de registro
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "REGISTER")
                client._enviar_cadena(sc, user)
                res = client._leer_byte(sc)
                
                # Mostramos por pantalla el resultado de la operación
                if res == 0:
                    print("c> REGISTER OK")
                    return client.RC.OK
                elif res == 1:
                    print("c> USERNAME IN USE")
                    return client.RC.USER_ERROR
                else:
                    print("c> REGISTER FAIL")
                    return client.RC.ERROR
        except Exception:
            print(f"c> REGISTER FAIL")
            return client.RC.ERROR
        return client.RC.ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 * 
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
        try: 
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                # Se conecta al servidor y envía la petición de darse de baja
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "UNREGISTER")
                client._enviar_cadena(sc, user)
                res = client._leer_byte(sc)
                
                # Mostramos por pantalla el resultado de la operación
                if res == 0:
                    print("c> UNREGISTER OK")
                    return client.RC.OK
                elif res == 1:
                    print("c> USERNAME NOT FOUND")
                    return client.RC.USER_ERROR
                else:
                    print("c> UNREGISTER FAIL")
                    return client.RC.ERROR
        except Exception:
            print("c> UNREGISTER FAIL")
        return client.RC.ERROR

    # *
    # * @param user - User name to connect to the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        if client._usuario_actual is not None:
            print("c> CONNECT FAIL")
            return client.RC.ERROR
            
        try:
            client._sc_escucha = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client._sc_escucha.bind(('', 0))
            client._sc_escucha.listen(5)
            puerto_str = str(client._sc_escucha.getsockname()[1])
            
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "CONNECT")
                client._enviar_cadena(sc, user)
                client._enviar_cadena(sc, puerto_str)
                res = client._leer_byte(sc)
                
                if res == 0:
                    client._usuario_actual = user
                    client._stop_event.clear()
                    client._hilo_escucha = threading.Thread(target=client._tratar_mensajes, daemon=True)
                    client._hilo_escucha.start()
                    print("c> CONNECT OK")
                    return client.RC.OK
                elif res == 1:
                    print("c> CONNECT FAIL, USER DOES NOT EXIST")
                    client._sc_escucha.close()
                    return client.RC.USER_ERROR
                elif res == 2:
                    print("c> USER ALREADY CONNECTED")
                    client._sc_escucha.close()
                    return client.RC.USER_ERROR
                else:
                    print("c> CONNECT FAIL")
                    client._sc_escucha.close()
                    return client.RC.ERROR
        except Exception:
            print("c> CONNECT FAIL")
            if client._sc_escucha:
                client._sc_escucha.close()
            return client.RC.ERROR
        return client.RC.ERROR

    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "USERS")
                
                if client._usuario_actual: 
                    client._enviar_cadena(sc, client._usuario_actual)
                else:
                    # Si no está conectado pero lanza el comando, mandamos string vacío para que el server conteste con error.
                    client._enviar_cadena(sc, "")
                
                res = client._leer_byte(sc)
                
                if res == 0:
                    num_conectados_str = client._leer_cadena(sc)
                    num_conectados = int(num_conectados_str)
                    print(f"c> CONNECTED USERS ({num_conectados} users connected) OK")
                    for _ in range(num_conectados):
                        informacion_usuario = client._leer_cadena(sc)
                        print(informacion_usuario)

                        # Separamos y guardamos la información aprovechando el formato
                        partes = informacion_usuario.split('::')
                        if len(partes) >= 3:
                            nombre = partes[0].strip()
                            ip = partes[1].strip()
                            puerto = int(partes[2].strip())
                            client._diccionario_usuarios[nombre] = (ip, puerto)

                    return client.RC.OK
                elif res == 1:
                    print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED") 
                    return client.RC.USER_ERROR
                else:
                    print("c> CONNECTED USERS FAIL")
                    return client.RC.ERROR
        except Exception:
            print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR
        return client.RC.ERROR

    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "DISCONNECT")
                client._enviar_cadena(sc, user)
                res = client._leer_byte(sc)
                
                # Cerrar el hilo pase lo que pase
                if client._usuario_actual == user:
                    client._stop_event.set()
                    if client._sc_escucha:
                        client._sc_escucha.close()
                    client._usuario_actual = None

                if res == 0:
                    print("c> DISCONNECT OK")
                    return client.RC.OK
                elif res == 1:
                    print("c> DISCONNECT FAIL, USER DOES NOT EXIST")
                    return client.RC.USER_ERROR
                elif res == 2:
                    print("c> DISCONNECT FAIL, USER NOT CONNECTED")
                    return client.RC.USER_ERROR
                else:
                    print("c> DISCONNECT FAIL")
                    return client.RC.ERROR
        except Exception:
            print("c> DISCONNECT FAIL")
            # Forzamos el cierre del hilo en caso de error
            client._stop_event.set()
            if client._sc_escucha:
                client._sc_escucha.close()
            client._usuario_actual = None
            return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        if client._usuario_actual is None:
            print("c> SEND FAIL")
            return client.RC.ERROR
            
        # Limitamos mensaje a 255 caracteres
        if len(message) > 255:
            message = message[:255]
            
        try:
            # Antes de enviar el mensaje, lo pasamos por el servicio web para normalizarlo
            wsdl = "http://localhost:8000/?wsdl"
            cliente_ws = zeep.Client(wsdl=wsdl)
            message = cliente_ws.service.normalizar(message)

            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "SEND")
                client._enviar_cadena(sc, client._usuario_actual)
                client._enviar_cadena(sc, user)
                client._enviar_cadena(sc, message)
                res = client._leer_byte(sc)
                
                if res == 0:
                    id_str = client._leer_cadena(sc)
                    print(f"c> SEND OK MESSAGE {id_str}")
                    return client.RC.OK
                elif res == 1:
                    print("c> SEND FAIL, USER DOES NOT EXIST")
                    return client.RC.USER_ERROR
                else:
                    print("c> SEND FAIL")
                    return client.RC.ERROR
        except Exception:
            print("c> SEND FAIL")
            return client.RC.ERROR
        return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user,  file,  message) :
        if client._usuario_actual is None:
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR

        # Limitamos mensaje y nombre de fichero a 255 caracteres
        if len(message) > 255:
            message = message[:255]
        if len(file) > 255:
            file = file[:255]

        try:
            # Antes de enviar el mensaje, lo pasamos por el servicio web para normalizarlo
            wsdl = "http://localhost:8000/?wsdl"
            cliente_ws = zeep.Client(wsdl=wsdl)
            message = cliente_ws.service.normalizar(message)

            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((client._server, client._port))
                client._enviar_cadena(sc, "SENDATTACH")
                client._enviar_cadena(sc, client._usuario_actual)
                client._enviar_cadena(sc, user)
                client._enviar_cadena(sc, message)
                client._enviar_cadena(sc, file)
                res = client._leer_byte(sc)

                if res == 0:
                    id_str = client._leer_cadena(sc)
                    print(f"c> SENDATTACH OK MESSAGE {id_str}")
                    return client.RC.OK
                elif res == 1:
                    print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
                    return client.RC.USER_ERROR
                else:
                    print("c> SENDATTACH FAIL")
                    return client.RC.ERROR
        except Exception:
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR
        
    # Función para solicitar un fichero a otro cliente
    @staticmethod
    def getfile(user, file, localFileName):
        if client._usuario_actual is None:
            # En el caso de no estar conectado ocurre un error
            print("c> FILE TRANSFER FAILED, user not connected.")
            return client.RC.ERROR

        # Comprobamos si el usuario está en el diccionario
        if user not in client._diccionario_usuarios:
            # Si no se encuentra probamos a hacer una petición
            client.users() 

        # Volvemos a comprobar si tras el refresco lo hemos encontrado
        if user not in client._diccionario_usuarios:
            # Si no vuelve a estar si que es un error
            print("c> FILE TRANSFER FAILED, user not connected.")
            return client.RC.USER_ERROR

        ip_destino, puerto_destino = client._diccionario_usuarios[user]

        try:
            # Nos conectamos al hilo de escucha del otro cliente que actúa como servidor para solicitar el fichero
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sc:
                sc.connect((ip_destino, puerto_destino))
                client._enviar_cadena(sc, "GET FILE")
                client._enviar_cadena(sc, client._usuario_actual)
                client._enviar_cadena(sc, file)

                # Recibimos el contenido y lo escribimos en el fichero indicado
                with open(localFileName, "wb") as fichero:
                    while True:
                        datos = sc.recv(4096)
                        if not datos:
                            # Si no recibimos datos, se ha terminado la transferencia
                            break
                        fichero.write(datos)
                
            return client.RC.OK
        except Exception:
            # Si falla la conexión
            print("c> FILE TRANSFER FAILED, user not connected.") 
            return client.RC.ERROR

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            #  Remove first two words
                            file = line[-1]
                            message = ' '.join(line[2:-1])
                            client.sendAttach(line[1], file, message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <message> <filename>")

                    elif(line[0]=="GETFILE") :
                        if (len(line) == 4) :
                            # GETFILE <userName> <fileName> <localFileName>
                            client.getfile(line[1], line[2], line[3])
                        else :
                            print("Syntax error. Usage: GETFILE <userName> <fileName> <localFileName>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # Si se sale de forma brusca pero estaba conectado, detenemos la escucha
                            if client._usuario_actual is not None:
                                client.disconnect(client._usuario_actual)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])
