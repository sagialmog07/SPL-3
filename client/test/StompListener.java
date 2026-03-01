import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/**
 * Enhanced Passive Listener Client for SPL3.
 * Usage: java StompListener <channel_name>
 * Example: java StompListener Germany_Japan
 */
public class StompListener {
    public static void main(String[] args) {
        // Default to Germany_Japan based on your events1.json
        String channel = (args.length > 0) ? args[0] : "Germany_Japan";
        
        // Ensure the channel starts with a '/' for STOMP standards
        if (!channel.startsWith("/")) {
            channel = "/" + channel;
        }

        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. Send CONNECT frame
            System.out.println("Connecting to server...");
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:listener_user\npasscode:123\n\n");
            
            String connectResponse = readFrame(in);
            if (connectResponse != null && connectResponse.startsWith("CONNECTED")) {
                System.out.println("CONNECTED to server successfully.");
            }

            // 2. Subscribe with a Receipt request to ensure sync
            String subId = "sub-" + System.currentTimeMillis();
            System.out.println("Subscribing to channel: " + channel + "...");
            sendFrame(out, "SUBSCRIBE\ndestination:" + channel + "\nid:" + subId + "\nreceipt:777\n\n");
            
            // Wait for Receipt
            String receipt = readFrame(in);
            if (receipt != null && receipt.contains("receipt-id:777")) {
                System.out.println("Successfully SUBSCRIBED. Waiting for messages on " + channel + "...");
            }

            // 3. Continuous listening loop
            while (true) {
                String frame = readFrame(in);
                if (frame == null) {
                    System.out.println("Connection closed by server.");
                    break;
                }
                if (!frame.isEmpty()) {
                    System.out.println("\n[RECEIVE MESSAGE FROM SERVER] --------\n" + frame + "\n--------------------------------------");
                }
            }
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
        }
    }

    private static void sendFrame(OutputStream out, String f) throws IOException {
        out.write((f + "\0").getBytes(StandardCharsets.UTF_8));
        out.flush();
    }

    private static String readFrame(InputStream in) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        int ch;
        while ((ch = in.read()) != 0) {
            if (ch == -1) return null;
            buffer.write(ch);
        }
        return buffer.toString(StandardCharsets.UTF_8.name());
    }
}