package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessageEncoderDecoder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class StompFrameEncoderDecoder implements MessageEncoderDecoder<String> {
    
    private byte[] bytes = new byte[1 << 10]; // 1KB initial buffer
    private int len = 0;

    @Override
    public String decodeNextByte(byte nextByte) {
        // STOMP frames are terminated by null byte '\0'
        if (nextByte == '\0') {
            return popString();
        }

        // Expand buffer if needed
        if (len >= bytes.length) {
            bytes = Arrays.copyOf(bytes, len * 2);
        }

        bytes[len++] = nextByte;
        return null; // Frame not complete yet
    }

    @Override
    public byte[] encode(String message) {
        // STOMP frames are terminated with null byte
        return (message + '\0').getBytes(StandardCharsets.UTF_8);
    }

    private String popString() {
        String result = new String(bytes, 0, len, StandardCharsets.UTF_8);
        len = 0;
        return result;
    }
}
