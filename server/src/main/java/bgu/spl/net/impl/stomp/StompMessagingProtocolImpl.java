package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArraySet;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {

    private int connectionId;
    private ConnectionsImpl<String> connections;
    private boolean shouldTerminate = false;
    private Database database;
    private boolean isLoggedIn = false;

    // Map subscription ID to channel/destination
    private final Map<String, String> subscriptions = new HashMap<>();

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = (ConnectionsImpl<String>) connections;
        this.database = Database.getInstance();
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    @Override
    public void process(String msg) {
        System.out.println("Processing message:\n" + msg);
        StompFrame frame = StompFrame.parse(msg);

        switch (frame.getCommand()) {
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
                sendError("Unknown command: " + frame.getCommand(), "");
        }

    }

    private void handleConnect(StompFrame frame) {
        String login = frame.getHeader("login");
        String passcode = frame.getHeader("passcode");

        if (login == null || passcode == null) {
            sendError("Missing credentials", "");
            connections.disconnect(connectionId);
            return;
        }

        LoginStatus status = database.login(connectionId, login, passcode);

        if (status == LoginStatus.LOGGED_IN_SUCCESSFULLY || status == LoginStatus.ADDED_NEW_USER) {
            this.isLoggedIn = true;

            StompFrame response = new StompFrame("CONNECTED");
            response.addHeader("version", "1.2");
            connections.send(connectionId, response.toString());
        } else {
            String errorMsg = getLoginErrorMessage(status);
            sendError(errorMsg, "");
            connections.disconnect(connectionId);
        }
    }

    private void handleSubscribe(StompFrame frame) {
        if (!isLoggedIn) {
            sendError("Not connected", "");
            return;
        }

        String destination = frame.getHeader("destination");
        String id = frame.getHeader("id");

        if (destination == null || id == null) {
            sendError("Missing destination or id header", "");
            return;
        }

        subscriptions.put(id, destination);
        connections.subscribe(destination, connectionId, id);

        String receiptId = frame.getHeader("receipt");
        if (receiptId != null) {
            sendReceipt(receiptId);
        }
    }

    private void handleUnsubscribe(StompFrame frame) {
        if (!isLoggedIn) {
            sendError("Not connected", "");
            return;
        }

        String id = frame.getHeader("id");

        if (id == null) {
            sendError("Missing id header", "");
            return;
        }

        String destination = subscriptions.remove(id);
        if (destination != null) {
            connections.unsubscribe(destination, connectionId);
        }

        String receiptId = frame.getHeader("receipt");
        if (receiptId != null) {
            sendReceipt(receiptId);
        }
    }

    private void handleSend(StompFrame frame) {
        if (!isLoggedIn) {
            sendError("Not connected", "");
            return;
        }

        String destination = frame.getHeader("destination");
        String fileName = frame.getHeader("file-name");
        String userName = frame.getHeader("user-name");
        database.trackFileUpload(userName, fileName, destination);

        if (destination == null) {
            sendError("Missing destination header", "");
            return;
        }

        // Get all subscribers for this channel
        CopyOnWriteArraySet<Integer> subscribers = connections.getSubscribers(destination);
        if (subscribers != null) {
            String messageId = String.valueOf(System.currentTimeMillis());
            
            for (Integer subscriberId : subscribers) {
                // Get the subscription ID for this specific subscriber
                String subId = connections.getSubscriptionId(subscriberId, destination);
                if (subId != null) {
                    // Create MESSAGE frame with correct subscription ID
                    StompFrame messageFrame = new StompFrame("MESSAGE");
                    messageFrame.addHeader("subscription", subId);
                    messageFrame.addHeader("message-id", messageId);
                    messageFrame.addHeader("destination", destination);
                    messageFrame.setBody(frame.getBody());
                    
                    // Send to this subscriber
                    connections.send(subscriberId, messageFrame.toString());
                }
            }
        }

        String receiptId = frame.getHeader("receipt");
        if (receiptId != null) {
            sendReceipt(receiptId);
        }
    }

    private void handleDisconnect(StompFrame frame) {
        String receiptId = frame.getHeader("receipt");
        if (receiptId != null) {
            sendReceipt(receiptId);
        }

        if (isLoggedIn) {
            database.logout(connectionId);
            isLoggedIn = false;
        }

        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    private void sendReceipt(String receiptId) {
        StompFrame receipt = new StompFrame("RECEIPT");
        receipt.addHeader("receipt-id", receiptId);
        connections.send(connectionId, receipt.toString());
    }

    private void sendError(String message, String details) {
        StompFrame error = new StompFrame("ERROR");
        error.addHeader("message", message);
        error.setBody(details);
        connections.send(connectionId, error.toString());
        
        // Per STOMP protocol, server must close connection after sending ERROR
        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    private String getLoginErrorMessage(LoginStatus status) {
        switch (status) {
            case ALREADY_LOGGED_IN:
                return "User already logged in";
            case WRONG_PASSWORD:
                return "Wrong password";
            case CLIENT_ALREADY_CONNECTED:
                return "Client already connected";
            default:
                return "Login failed";
        }
    }

    /**
     * Helper class to parse and build STOMP frames
     */
    private static class StompFrame {
        private String command;
        private final Map<String, String> headers = new HashMap<>();
        private String body = "";

        public StompFrame(String command) {
            this.command = command;
        }

        public String getCommand() {
            return command;
        }

        public void addHeader(String key, String value) {
            headers.put(key, value);
        }

        public String getHeader(String key) {
            return headers.get(key);
        }

        public void setBody(String body) {
            this.body = body != null ? body : "";
        }

        public String getBody() {
            return body;
        }

        public static StompFrame parse(String frameString) {
            String[] lines = frameString.split("\n", -1);

            if (lines.length == 0) {
                throw new IllegalArgumentException("Empty frame");
            }

            StompFrame frame = new StompFrame(lines[0]);

            int i = 1;
            // Parse headers
            while (i < lines.length && !lines[i].isEmpty()) {
                String line = lines[i];
                int colonIndex = line.indexOf(':');
                if (colonIndex > 0) {
                    String key = line.substring(0, colonIndex);
                    String value = line.substring(colonIndex + 1);
                    frame.addHeader(key, value);
                }
                i++;
            }

            // Skip empty line between headers and body
            i++;

            // Parse body (everything after the empty line)
            if (i < lines.length) {
                StringBuilder bodyBuilder = new StringBuilder();
                for (int j = i; j < lines.length; j++) {
                    if (j > i)
                        bodyBuilder.append("\n");
                    bodyBuilder.append(lines[j]);
                }
                frame.setBody(bodyBuilder.toString());
            }

            return frame;
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append(command).append("\n");

            for (Map.Entry<String, String> entry : headers.entrySet()) {
                sb.append(entry.getKey()).append(":").append(entry.getValue()).append("\n");
            }

            sb.append("\n");
            
            if (body != null && !body.isEmpty()) {
                sb.append(body);
            }
            // Note: Null terminator is added by StompFrameEncoderDecoder.encode()

            return sb.toString();
        }

    }
}