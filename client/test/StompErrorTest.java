import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/**
 * Negative Test Case.
 * Validates that sending a frame to an unauthorized destination triggers a STOMP ERROR 
 * and closes the connection.
 */
public class StompErrorTest {
    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. Establish valid connection
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:ofek\npasscode:123\n\n");
            readFrame(in);

            // 2. Illegal Action: Send to a topic without joining
            System.out.println("--- Sending to unauthorized topic (Expected ERROR) ---");
            sendFrame(out, "SEND\ndestination:/secret_channel\n\nThis should fail\n");

            // 3. Read ERROR frame and verify socket closure
            String response = readFrame(in);
            if (response.startsWith("ERROR")) {
                System.out.println("Success: Server returned ERROR as expected.");
                if (in.read() == -1) {
                    System.out.println("Success: Server closed the socket correctly.");
                }
            }

        } catch (Exception e) {
            System.out.println("Test ended: " + e.getMessage());
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
        return sb.toString();
    }
}