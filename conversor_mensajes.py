import logging
from wsgiref.simple_server import make_server
from spyne import Application, ServiceBase, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication

class ConversorMensajes(ServiceBase):
    # Recibe mensajes de texto y devuelve también mensajes de texto
    @rpc(Unicode, _returns=Unicode)
    def normalizar(ctx, texto):
        # Aquí implementamos el código para normalizar el mensaje
        # Con split lo dividimos en palabras y con join lo juntamos con un solo espacio entre palabras
        return ' '.join(texto.split())

# Configuración de la aplicación
application = Application(services=[ConversorMensajes], tns='http://tests.python-zeep.org/'
                          ,in_protocol=Soap11(validator='lxml'), out_protocol=Soap11())
application = WsgiApplication(application)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    logging.getLogger('spyne.protocol.xml').setLevel(logging.DEBUG)
    logging.info("listening to http://127.0.0.1:8000; wsdl is at: http://localhost:8000/?wsdl")
    # Creamos el servidor escuchando en el puerto 8000
    server = make_server('127.0.0.1', 8000, application)
    server.serve_forever()