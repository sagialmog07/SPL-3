package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.srv.ConnectionsImpl;
import bgu.spl.net.srv.Server;
import java.util.concurrent.atomic.AtomicInteger;

public class StompServer {

    private static final ConnectionsImpl<String> connections = new ConnectionsImpl<>();
    private static final AtomicInteger connectionIdCounter = new AtomicInteger(0);

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: java StompServer <port> [server_type]");
            System.err.println("  server_type: tpc (thread-per-client) or reactor (default: tpc)");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args.length > 1 ? args[1] : "tpc";

        System.out.println("Starting STOMP Server on port " + port + " with " + serverType + " pattern");

        if ("reactor".equalsIgnoreCase(serverType)) {
            // Reactor pattern - multiple threads handling connections
            Server.<String>reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    StompServer::createProtocol,
                    StompFrameEncoderDecoder::new
            ).serve();
        } else {
            // Thread-per-client pattern (default)
            Server.<String>threadPerClient(
                    port,
                    StompServer::createProtocol,
                    StompFrameEncoderDecoder::new
            ).serve();
        }
    }

    private static MessagingProtocol<String> createProtocol() {
        int connectionId = connectionIdCounter.getAndIncrement();
        StompMessagingProtocolImpl protocol = new StompMessagingProtocolImpl();
        protocol.start(connectionId, connections);
        return new StompProtocolAdapter(protocol, connectionId, connections);
    }
}
