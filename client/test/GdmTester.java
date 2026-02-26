import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/**
 * Active Reporter Client.
 * Connects and sends a World Cup game event in the required multi-line body format.
 */
public class GdmTester {
    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. Authenticate
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:reporter_user\npasscode:456\n\n");
            readFrame(in);

            // 2. Send World Cup Report
            System.out.println("Sending game event to /usa_canada...");
            String report = "user: reporter_user\n" +
                            "team a: usa\n" +
                            "team b: canada\n" +
                            "event name: kickoff\n" +
                            "time: 0\n" +
                            "general game updates:\n" +
                            "active: true\n" +
                            "before halftime: true\n" +
                            "team a updates:\n" +
                            "team b updates:\n" +
                            "description:\n" +
                            "The match has officially started!\n";

            sendFrame(out, "SEND\ndestination:/usa_canada\n\n" + report);
            
            Thread.sleep(500); 
            System.out.println("Report sent successfully.");

        } catch (Exception e) { e.printStackTrace(); }
    }

    private static void sendFrame(OutputStream out, String f) throws IOException {
        out.write((f + "\0").getBytes(StandardCharsets.UTF_8));
        out.flush();
    }

    private static String readFrame(InputStream in) throws IOException {
        int ch;
        StringBuilder sb = new StringBuilder();
        while ((ch = in.read()) != 0 && ch != -1) {
            sb.append((char)ch);
        }
        return sb.toString();
    }
}