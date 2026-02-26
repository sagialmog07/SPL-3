#!/usr/bin/env python3
import socket
import sys
import threading
import sqlite3
import os

SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"  # DO NOT CHANGE!
DB_FILE = "stomp_server.db"              # DO NOT CHANGE!

# Global database lock for thread safety 
db_lock = threading.Lock()

def recv_null_terminated(sock: socket.socket) -> str:
    """Reads data from socket until a null character is found """
    data = b""
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return ""
        data += chunk
        if b"\0" in data:
            msg, _ = data.split(b"\0", 1)
            return msg.decode("utf-8", errors="replace")

def init_database():
    """Initialize SQLite database with required tables according to Database.java """
    with db_lock:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        
        # 1. Users table: stores registration info [cite: 12, 15]
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS users (
                username TEXT PRIMARY KEY,
                password TEXT NOT NULL,
                registration_date DATETIME
            )
        """)
        
        # 2. Login history: tracks login and logout events [cite: 12, 15]
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS login_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT,
                login_time DATETIME,
                logout_time DATETIME,
                FOREIGN KEY (username) REFERENCES users(username)
            )
        """)
        
        # 3. File tracking: logs file uploads per user and channel [cite: 12, 15]
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS file_tracking (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT,
                filename TEXT,
                upload_time DATETIME,
                game_channel TEXT,
                FOREIGN KEY (username) REFERENCES users(username)
            )
        """)
        
        conn.commit()
        conn.close()
        print(f"[{SERVER_NAME}] Database initialized: {DB_FILE}")

def execute_sql_command(sql_command: str) -> str:
    """Executes INSERT or UPDATE commands and returns 'done' as per PDF [cite: 12, 15]"""
    with db_lock:
        conn = None
        try:
            conn = sqlite3.connect(DB_FILE, timeout=30.0)
            cursor = conn.cursor()                    
            cursor.execute(sql_command)
            conn.commit()
            return "done"
        except sqlite3.Error as e:
            if conn: conn.rollback()
            return f"ERROR:{e}"
        finally:
            if conn: conn.close()

def execute_sql_query(sql_query: str) -> str:
    """Executes SELECT and returns formatted results for printReport() in Java """
    with db_lock:
        conn = None
        try:
            conn = sqlite3.connect(DB_FILE, timeout=30.0)
            cursor = conn.cursor()
            cursor.execute(sql_query)
            rows = cursor.fetchall()
            
            # Format: SUCCESS|str(row1)|str(row2)... as expected by Database.java 
            if rows:
                result_parts = ["SUCCESS"]
                for row in rows:
                    result_parts.append(str(row))
                return "|".join(result_parts)
            else:
                return "SUCCESS"
        except sqlite3.Error as e:
            return f"ERROR:{e}"
        finally:
            if conn: conn.close()

def handle_client(client_socket: socket.socket, addr):
    """Processes incoming SQL strings from the Java server """
    try:
        while True:
            message = recv_null_terminated(client_socket)
            if not message: break

            print(f"[{SERVER_NAME}] Executing SQL: {message}")

            # Routing based on SQL command type 
            sql_upper = message.strip().upper()
            if sql_upper.startswith("SELECT"):
                response = execute_sql_query(message)
            else:
                response = execute_sql_command(message)
            
            client_socket.sendall(response.encode('utf-8') + b"\0")
    except Exception as e:
        print(f"[{SERVER_NAME}] Client Error: {e}")
    finally:
        client_socket.close()

def start_server(host="127.0.0.1", port=7778):
    init_database()
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"[{SERVER_NAME}] Listening on {host}:{port}")

        while True:
            client_socket, addr = server_socket.accept()
            threading.Thread(target=handle_client, args=(client_socket, addr), daemon=True).start()
    except KeyboardInterrupt:
        print(f"[{SERVER_NAME}] Shutting down...")
    finally:
        server_socket.close()

if __name__ == "__main__":
    start_server()