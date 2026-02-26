package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.ConnectionHandler;
import bgu.spl.net.srv.ConnectionsImpl;

/**
 * Adapter to bridge StompMessagingProtocol (which has void process) 
 * to MessagingProtocol (which returns a response)
 */
public class StompProtocolAdapter implements MessagingProtocol<String> {
    
    private final StompMessagingProtocol<String> stompProtocol;
    private final int connectionId;
    private final ConnectionsImpl<String> connections;
    private ConnectionHandler<String> handler;
    
    public StompProtocolAdapter(StompMessagingProtocol<String> stompProtocol, int connectionId, ConnectionsImpl<String> connections) {
        this.stompProtocol = stompProtocol;
        this.connectionId = connectionId;
        this.connections = connections;
    }
    
    public void setHandler(ConnectionHandler<String> handler) {
        this.handler = handler;
        connections.addConnection(connectionId, handler);
        System.out.println("Registered connection handler for connectionId: " + connectionId);
    }
    
    @Override
    public String process(String msg) {
        stompProtocol.process(msg);
        return null; // STOMP protocol sends responses through Connections interface
    }
    
    @Override
    public boolean shouldTerminate() {
        return stompProtocol.shouldTerminate();
    }
}
