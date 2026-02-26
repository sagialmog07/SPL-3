#!/usr/bin/env python3
"""
Basic Python Server for STOMP Assignment – Stage 3.3

IMPORTANT:
DO NOT CHANGE the server name or the basic protocol.
Students should EXTEND this server by implementing
the methods below.
"""

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
    """Initialize SQLite database with required tables"""
    with db_lock:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        
        # Create users table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS users (
                username TEXT PRIMARY KEY,
                password TEXT NOT NULL,
                registration_date DATETIME
            )
        """)
        
        # Create login_history table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS login_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT,
                login_time DATETIME,
                logout_time DATETIME,
                FOREIGN KEY (username) REFERENCES users(username)
            )
        """)
        
        # Create file_tracking table (note: Database.java uses this name)
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
    """Execute INSERT, UPDATE, DELETE commands"""
    with db_lock:
        conn = None
        try:
            conn = sqlite3.connect(DB_FILE, timeout=30.0)
            cursor = conn.cursor()                    
            
            cursor.execute(sql_command)
            conn.commit()
            return "done"
        except sqlite3.IntegrityError as e:
            if conn:
                conn.rollback()
            if "UNIQUE constraint failed" in str(e):
                print(f"[{SERVER_NAME}] Duplicate entry: {e}")
                return "ERROR:DUPLICATE"
            print(f"[{SERVER_NAME}] Integrity Error: {e}")
            return f"ERROR:INTEGRITY:{e}"
        except sqlite3.Error as e:
            if conn:
                conn.rollback()
            print(f"[{SERVER_NAME}] SQL Command Error: {e}")
            return f"ERROR:{e}"
        except Exception as e:
            if conn:
                conn.rollback()
            print(f"[{SERVER_NAME}] Unexpected Error: {e}")
            return f"ERROR:{e}"
        finally:
            if conn:
                conn.close()

def execute_sql_query(sql_query: str) -> str:
    """Execute SELECT queries and return formatted results"""
    with db_lock:
        conn = None
        try:
            conn = sqlite3.connect(DB_FILE, timeout=30.0)
            cursor = conn.cursor()
            cursor.execute(sql_query)
            rows = cursor.fetchall()
            
            if rows:
                result_parts = ["SUCCESS"]
                for row in rows:
                    result_parts.append(str(row))
                return "|".join(result_parts)
            else:
                return "SUCCESS"
                
        except sqlite3.Error as e:
            print(f"[{SERVER_NAME}] SQL Query Error: {e}")
            return f"ERROR:{e}"
        except Exception as e:
            print(f"[{SERVER_NAME}] Unexpected Error: {e}")
            return f"ERROR:{e}"
        finally:
            if conn:
                conn.close()

def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Client connected from {addr}")

    try:
        while True:
            message = recv_null_terminated(client_socket)
            if message == "":
                break

            print(f"[{SERVER_NAME}] Received SQL:")
            print(message)

            # Determine if it's a query (SELECT) or command (INSERT/UPDATE/DELETE)
            sql_upper = message.strip().upper()
            
            if sql_upper.startswith("SELECT"):
                response = execute_sql_query(message)
            elif sql_upper.startswith(("INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "ALTER")):
                response = execute_sql_command(message)
            else:
                response = "ERROR:Unknown SQL command"
            
            print(f"[{SERVER_NAME}] Sending response: {response}")
            client_socket.sendall(response.encode('utf-8') + b"\0")

    except Exception as e:
        print(f"[{SERVER_NAME}] Error handling client {addr}: {e}")
    finally:
        try:
            client_socket.close()
        except Exception:
            pass
        print(f"[{SERVER_NAME}] Client {addr} disconnected")


def start_server(host="127.0.0.1", port=7778):
    # Initialize database before starting server
    init_database()
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.settimeout(1.0)  # Set timeout to allow Ctrl+C to work on Windows

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"[{SERVER_NAME}] Server started on {host}:{port}")
        print(f"[{SERVER_NAME}] Waiting for connections...")

        while True:
            try:
                client_socket, addr = server_socket.accept()
                t = threading.Thread(
                    target=handle_client,
                    args=(client_socket, addr),
                    daemon=True
                )
                t.start()
            except socket.timeout:
                continue  # Timeout allows checking for KeyboardInterrupt

    except KeyboardInterrupt:
        print(f"\n[{SERVER_NAME}] Shutting down server...")
    finally:
        try:
            server_socket.close()
        except Exception:
            pass



if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
