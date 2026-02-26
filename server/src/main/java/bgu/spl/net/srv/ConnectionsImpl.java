package bgu.spl.net.srv;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.CopyOnWriteArraySet;

public class ConnectionsImpl<T> implements Connections<T> {
    
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> connectionHandlers;
    private final ConcurrentHashMap<String, CopyOnWriteArraySet<Integer>> channelSubscriptions;
    // Track subscriptions: connectionId -> List of Subscriptions
    private final ConcurrentHashMap<Integer, CopyOnWriteArrayList<Subscription>> connectionSubscriptions;

    public ConnectionsImpl() {
        this.connectionHandlers = new ConcurrentHashMap<>();
        this.channelSubscriptions = new ConcurrentHashMap<>();
        this.connectionSubscriptions = new ConcurrentHashMap<>();
    }

    /**
     * Register a new connection handler
     */
    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        connectionHandlers.put(connectionId, handler);
    }

    /**
     * Subscribe a connection to a channel with a specific subscription ID
     */
    public void subscribe(String channel, int connectionId, String subscriptionId) {
        // Add to channel subscribers
        channelSubscriptions.computeIfAbsent(channel, k -> new CopyOnWriteArraySet<>()).add(connectionId);
        
        // Track subscription details
        Subscription subscription = new Subscription(subscriptionId, channel, connectionId);
        connectionSubscriptions.computeIfAbsent(connectionId, k -> new CopyOnWriteArrayList<>()).add(subscription);
    }

    /**
     * Unsubscribe a connection from a channel
     */
    public void unsubscribe(String channel, int connectionId) {
        // Remove from channel subscribers
        CopyOnWriteArraySet<Integer> subscribers = channelSubscriptions.get(channel);
        if (subscribers != null) {
            subscribers.remove(connectionId);
            if (subscribers.isEmpty()) {
                channelSubscriptions.remove(channel);
            }
        }
        
        // Remove subscription from connection's subscription list
        CopyOnWriteArrayList<Subscription> subs = connectionSubscriptions.get(connectionId);
        if (subs != null) {
            subs.removeIf(sub -> sub.getChannel().equals(channel));
            if (subs.isEmpty()) {
                connectionSubscriptions.remove(connectionId);
            }
        }
    }

    @Override
    public boolean send(int connectionId, T msg) {
        System.out.println("Sending message to connection " + connectionId + ":\n" + msg);
        ConnectionHandler<T> handler = connectionHandlers.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        CopyOnWriteArraySet<Integer> subscribers = channelSubscriptions.get(channel);
        if (subscribers != null) {
            for (Integer connId : subscribers) {
                send(connId, msg);
            }
        }
    }
    
    /**
     * Get subscription ID for a connection and channel
     */
    public String getSubscriptionId(int connectionId, String channel) {
        List<Subscription> subs = connectionSubscriptions.get(connectionId);
        if (subs != null) {
            for (Subscription sub : subs) {
                if (sub.getChannel().equals(channel)) {
                    return sub.getSubscriptionId();
                }
            }
        }
        return null;
    }

    @Override
    public void disconnect(int connectionId) {
        // Remove from all channel subscriptions
        for (CopyOnWriteArraySet<Integer> subscribers : channelSubscriptions.values()) {
            subscribers.remove(connectionId);
        }
        
        // Remove all subscriptions for this connection
        connectionSubscriptions.remove(connectionId);
        
        // Close and remove the connection handler
        ConnectionHandler<T> handler = connectionHandlers.remove(connectionId);
        if (handler != null) {
            try {
                handler.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public CopyOnWriteArraySet<Integer> getSubscribers(String channel) {
        return channelSubscriptions.get(channel);
    }
}
