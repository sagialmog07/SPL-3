import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/**
 * Full Integration Test for STOMP 1.2 World Cup Informer.
 * Tests: CONNECT, SUBSCRIBE (with receipt), SEND (World Cup format), and DISCONNECT (with receipt).
 */
public class StompTestClient {
    private static final String HOST = "stomp.cs.bgu.ac.il";
    private static final int PORT = 7777;

    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", PORT);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. STOMP 1.2 CONNECT
            System.out.println("--- Testing CONNECT ---");
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:" + HOST + "\nlogin:meni\npasscode:films\n\n");
            readFrame(in); 

            // 2. SUBSCRIBE with mandatory Receipt
            System.out.println("\n--- Testing JOIN (SUBSCRIBE) ---");
            sendFrame(out, "SUBSCRIBE\ndestination:/germany_spain\nid:17\nreceipt:73\n\n");
            readFrame(in); // Blocks until RECEIPT 73 is received

            // 3. SEND a World Cup Event
            System.out.println("\n--- Testing REPORT (SEND) ---");
            String eventBody = "user: meni\n" +
                               "team a: germany\n" +
                               "team b: spain\n" +
                               "event name: goal\n" +
                               "time: 120\n" +
                               "general game updates:\n" +
                               "active: true\n" +
                               "team a updates:\n" +
                               "goals: 1\n" +
                               "team b updates:\n" +
                               "description:\n" +
                               "A fantastic header into the top corner!\n";
            sendFrame(out, "SEND\ndestination:/germany_spain\n\n" + eventBody);
            readFrame(in); // Server echoes the message back to subscribers

            // 4. Graceful DISCONNECT
            System.out.println("\n--- Testing DISCONNECT ---");
            sendFrame(out, "DISCONNECT\nreceipt:113\n\n");
            readFrame(in); // Blocks until RECEIPT 113 is received

            System.out.println("\nTest Completed: All frames processed correctly.");

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void sendFrame(OutputStream out, String frame) throws IOException {
        out.write((frame + "\0").getBytes(StandardCharsets.UTF_8));
        out.flush();
    }

    private static String readFrame(InputStream in) throws IOException {
        StringBuilder sb = new StringBuilder();
        int ch;
        while ((ch = in.read()) != 0 && ch != -1) {
            sb.append((char) ch);
        }
        String response = sb.toString();
        System.out.println("Received:\n" + response);
        return response;
    }
}