import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/**
 * Passive Listener Client.
 * Subscribes to a channel and stays open to receive all incoming MESSAGE frames.
 */
public class StompListener {
    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. Connect and Subscribe
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:listener_user\npasscode:123\n\n");
            readFrame(in); 
            
            sendFrame(out, "SUBSCRIBE\ndestination:/usa_canada\nid:sub10\n\n");
            System.out.println("Listening on /usa_canada...");

            // 2. Continuous listening loop
            while (true) {
                String frame = readFrame(in);
                if (frame == null || frame.isEmpty()) {
                    System.out.println("Connection lost.");
                    break;
                }
                System.out.println("\n[RECEIVE] " + System.currentTimeMillis() + "\n" + frame);
            }
        } catch (Exception e) {
            e.printStackTrace();
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