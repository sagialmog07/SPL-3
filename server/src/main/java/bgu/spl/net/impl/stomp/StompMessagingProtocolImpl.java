package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {

    // Global counter for generating unique message IDs across the server
    private static final AtomicInteger messageIdCounter = new AtomicInteger(1);

    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    
    // Tracks the logged-in user's username for this specific connection
    private String username = null;
    
    // Maps the client's subscription ID to the topic name (destination)
    private Map<String, String> subIdToTopic = new HashMap<>();

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(String message) {
        // Parse the raw string message into a StompFrame object
        StompFrame frame = StompFrame.parse(message);
        if (frame == null) {
            return;
        }

        String command = frame.getCommand();

        // Route the frame to the appropriate handler method
        switch (command) {
            case "CONNECT":
                handleConnect(frame);
                break;
            case "SUBSCRIBE":
                handleSubscribe(frame);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(frame);
                break;
            case "SEND":
                handleSend(frame);
                break;
            case "DISCONNECT":
                handleDisconnect(frame);
                break;
            default:
                sendError("Unknown command", "The server only supports standard STOMP commands", frame);
                break;
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    private void handleConnect(StompFrame frame) {
        String acceptVersion = frame.getHeader("accept-version");
        String host = frame.getHeader("host");
        String login = frame.getHeader("login");
        String passcode = frame.getHeader("passcode");

        // Validate required headers
        if (acceptVersion == null || host == null || login == null || passcode == null) {
            sendError("Malformed frame", "CONNECT frame is missing required headers", frame);
            return;
        }

        // Attempt login/registration via Database singleton
        LoginStatus status = Database.getInstance().login(connectionId, login, passcode);

        switch (status) {
            case ADDED_NEW_USER:
            case LOGGED_IN_SUCCESSFULLY:
                this.username = login;
                Map<String, String> connectedHeaders = new HashMap<>();
                connectedHeaders.put("version", "1.2");
                StompFrame connectedFrame = new StompFrame("CONNECTED", connectedHeaders, null);
                connections.send(connectionId, connectedFrame.toString());
                break;
            case WRONG_PASSWORD:
                sendError("Wrong password", "The password provided is incorrect", frame);
                break;
            case ALREADY_LOGGED_IN:
            case CLIENT_ALREADY_CONNECTED:
                sendError("User already logged in", "This user is already connected to the server", frame);
                break;
        }
    }

    private void handleSubscribe(StompFrame frame) {
        String destination = frame.getHeader("destination");
        String id = frame.getHeader("id");

        if (destination == null || id == null) {
            sendError("Malformed frame", "SUBSCRIBE requires destination and id headers", frame);
            return;
        }

        // Track the subscription ID locally for this connection
        subIdToTopic.put(id, destination);

        // Register the subscription in the global connections manager
        if (connections instanceof ConnectionsImpl) {
            ((ConnectionsImpl<String>) connections).subscribe(destination, connectionId, id);
        }

        sendReceiptIfNeeded(frame);
    }

    private void handleUnsubscribe(StompFrame frame) {
        String id = frame.getHeader("id");

        if (id == null) {
            sendError("Malformed frame", "UNSUBSCRIBE requires an id header", frame);
            return;
        }

        // Remove the subscription tracking
        String topic = subIdToTopic.remove(id);

        if (topic != null && connections instanceof ConnectionsImpl) {
            ((ConnectionsImpl<String>) connections).unsubscribe(topic, connectionId);
        }

        sendReceiptIfNeeded(frame);
    }

    private void handleSend(StompFrame frame) {
        String destination = frame.getHeader("destination");

        if (destination == null) {
            sendError("Malformed frame", "SEND requires a destination header", frame);
            return;
        }

        // SECURITY CHECK: Ensure user is subscribed to the topic before allowing SEND
        if (!subIdToTopic.containsValue(destination)) {
            sendError("Not subscribed", "You cannot send messages to a topic you are not subscribed to: " + destination, frame);
            return;
        }
        
        // Track file uploads in Database if relevant headers are present
        String reportUser = frame.getHeader("user");
        if (reportUser != null) {
            String filename = frame.getHeader("file");
            if (filename == null) filename = "unknown_file";
            Database.getInstance().trackFileUpload(reportUser, filename, destination);
        }

        // Prepare the MESSAGE frame for broadcasting
        Map<String, String> messageHeaders = new HashMap<>();
        messageHeaders.put("destination", destination);
        messageHeaders.put("message-id", String.valueOf(messageIdCounter.getAndIncrement()));
        
        // The "subscription" header will be updated per-client by ConnectionsImpl
        messageHeaders.put("subscription", destination); 

        StompFrame messageFrame = new StompFrame("MESSAGE", messageHeaders, frame.getBody());
        
        // Broadcast to all subscribers of this topic
        connections.send(destination, messageFrame.toString());

        sendReceiptIfNeeded(frame);
    }

    private void handleDisconnect(StompFrame frame) {
        // According to STOMP, send receipt first if requested, then close
        sendReceiptIfNeeded(frame);

        if (username != null) {
        Database.getInstance().logout(connectionId); 
        }
       // if (connections instanceof ConnectionsImpl) {
         //   ((ConnectionsImpl<String>) connections).disconnect(connectionId);
        //}
        connections.disconnect(connectionId);
        shouldTerminate = true;
    }

    private void sendReceiptIfNeeded(StompFrame frame) {
        String receiptId = frame.getHeader("receipt");
        if (receiptId != null) {
            Map<String, String> receiptHeaders = new HashMap<>();
            receiptHeaders.put("receipt-id", receiptId);
            StompFrame receiptFrame = new StompFrame("RECEIPT", receiptHeaders, null);
            connections.send(connectionId, receiptFrame.toString());
        }
    }

    private void sendError(String messageHeader, String description, StompFrame causeFrame) {
        Map<String, String> errorHeaders = new HashMap<>();
        errorHeaders.put("message", messageHeader);
        
        // Include receipt-id in ERROR if the failing frame requested one
        String receiptId = causeFrame.getHeader("receipt");
        if (receiptId != null) {
            errorHeaders.put("receipt-id", receiptId);
        }

        // Build error frame with details about the causing frame
        StompFrame errorFrame = new StompFrame("ERROR", errorHeaders, description + "\n\nOriginal Frame:\n" + causeFrame.toString());
        connections.send(connectionId, errorFrame.toString());
        
        // IMPORTANT: STOMP protocol mandates closing the connection immediately after an ERROR
        if (connections instanceof ConnectionsImpl) {
            ((ConnectionsImpl<String>) connections).disconnect(connectionId);
        }
        
        shouldTerminate = true;
    }
}