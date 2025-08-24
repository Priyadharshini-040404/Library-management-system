#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;

// Global handles
SQLHENV env = NULL;
SQLHDBC dbc = NULL;
string currentUserRole;
void addBook();
void updateBook();
void deleteBook();
void viewMembers();
void searchMembers();



// Utility function to show SQL errors
void showError(const char* fn, SQLHANDLE handle, SQLSMALLINT type) {
    SQLINTEGER i = 0, native;
    SQLCHAR state[7], text[256];
    SQLSMALLINT len;
    while (SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len) == SQL_SUCCESS)
        cerr << fn << " error: " << state << " " << native << " " << text << "\n";
}

// Check if record exists
bool exists(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
}

// Execute SQL command
bool execSQL(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) showError("execSQL", stmt, SQL_HANDLE_STMT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(ret);
}
// ===== Login =====
bool login() {
    string username, password;
    cout << "\n=== Login ===\nUsername: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);

    string sql = "SELECT role FROM users WHERE username='" + username + 
             "' AND passwordhash='" + password + "'";


    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("Login", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }

    SQLCHAR role[20], status[20];
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, role, sizeof(role), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, status, sizeof(status), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (string((char*)status) == "Inactive") {
            cout << "Account is inactive.\n";
            return false;
        }

        currentUserRole = (char*)role;
        cout << "Login successful. Role: " << currentUserRole << "\n";
        return true;
    } else {
        cout << "Invalid credentials.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
}
// ===== Book Management =====
void addBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string title, author, genre, publisher, isbn, edition, rack, language;
    int year;
    double price;

    cout << "Title: "; getline(cin, title);
    cout << "Author: "; getline(cin, author);
    cout << "Genre: "; getline(cin, genre);
    cout << "Publisher: "; getline(cin, publisher);
    cout << "ISBN (unique): "; getline(cin, isbn);
    if (exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "ISBN already exists.\n"; return;
    }
    cout << "Edition: "; getline(cin, edition);
    cout << "Published Year: "; cin >> year; cin.ignore();
    cout << "Price: "; cin >> price; cin.ignore();
    cout << "Rack Location: "; getline(cin, rack);
    cout << "Language: "; getline(cin, language);

    string sql = "INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language, Availability) "
                 "VALUES (N'" + title + "', N'" + author + "', N'" + genre + "', N'" + publisher + "', '" + isbn + "', N'" + edition + "', " +
                 to_string(year) + ", " + to_string(price) + ", N'" + rack + "', N'" + language + "', 1)";

    cout << (execSQL(sql) ? "Book added successfully.\n" : "Failed to add book.\n");
}

void updateBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string isbn;
    cout << "Enter ISBN to update: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }

    string title, author, genre, publisher, edition, rack, language;
    int year = 0;
    double price = 0;

    cout << "New Title (leave blank to skip): "; getline(cin, title);
    cout << "New Author (leave blank to skip): "; getline(cin, author);
    cout << "New Genre (leave blank to skip): "; getline(cin, genre);
    cout << "New Publisher (leave blank to skip): "; getline(cin, publisher);
    cout << "New Edition (leave blank to skip): "; getline(cin, edition);
    cout << "New Published Year (0 to skip): "; cin >> year; cin.ignore();
    cout << "New Price (0 to skip): "; cin >> price; cin.ignore();
    cout << "New Rack Location (leave blank to skip): "; getline(cin, rack);
    cout << "New Language (leave blank to skip): "; getline(cin, language);

    string sql = "UPDATE Books SET ";
    bool first = true;

    if (!title.empty()) { sql += "Title=N'" + title + "'"; first = false; }
    if (!author.empty()) { sql += (first ? "" : ", ") + string("Author=N'") + author + "'"; first = false; }
    if (!genre.empty()) { sql += (first ? "" : ", ") + string("Genre=N'") + genre + "'"; first = false; }
    if (!publisher.empty()) { sql += (first ? "" : ", ") + string("Publisher=N'") + publisher + "'"; first = false; }
    if (!edition.empty()) { sql += (first ? "" : ", ") + string("Edition=N'") + edition + "'"; first = false; }
    if (year != 0) { sql += (first ? "" : ", ") + string("PublishedYear=") + to_string(year); first = false; }
    if (price != 0) { sql += (first ? "" : ", ") + string("Price=") + to_string(price); first = false; }
    if (!rack.empty()) { sql += (first ? "" : ", ") + string("RackLocation=N'") + rack + "'"; first = false; }
    if (!language.empty()) { sql += (first ? "" : ", ") + string("Language=N'") + language + "'"; }

    sql += " WHERE ISBN='" + isbn + "'";

    cout << (execSQL(sql) ? "Book updated successfully.\n" : "Update failed.\n");
}
void deleteBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string isbn;
    cout << "Enter ISBN to delete: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }
    if (exists("SELECT * FROM Transactions WHERE BookID=(SELECT BookID FROM Books WHERE ISBN='" + isbn + "') AND ReturnDate IS NULL")) {
        cout << "Book currently issued, cannot delete.\n"; return;
    }
    cout << (execSQL("DELETE FROM Books WHERE ISBN='" + isbn + "'") ? "Book deleted successfully.\n" : "Delete failed.\n");
}
void viewBooks() {
    int pageSize = 5, page = 0;
    string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT BookID, Title, Author, ISBN, Availability FROM books ORDER BY Title OFFSET " +
                     to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tTitle\tAuthor\tISBN\tAvailability\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id, avail;
                char title[255], author[200], isbn[20];
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
                SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
                SQLGetData(stmt, 5, SQL_C_LONG, &avail, 0, NULL);
                cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\t" << (avail ? "Yes" : "No") << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        cout << "[N]ext, [P]revious, [Q]uit: ";
        getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchBooks() {
    cout << "Enter keyword to search in title: ";
    string keyword; getline(cin, keyword);
    string sql = "SELECT BookID, Title, Author, ISBN FROM books WHERE Title LIKE N'%" + keyword + "%'";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tTitle\tAuthor\tISBN\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char title[255], author[200], isbn[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
            cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
// ===== Member Management =====
void addMember() {
    if (currentUserRole != "admin") { cout << "Access denied.\n"; return; }

    string name, email, phone, address, membership;
    cout << "Name: "; getline(cin, name);
    cout << "Email: "; getline(cin, email);
    if (exists("SELECT * FROM members WHERE email='" + email + "'")) { cout << "Email exists.\n"; return; }
    cout << "Phone: "; getline(cin, phone);
    cout << "Address: "; getline(cin, address);
    cout << "Membership Type: "; getline(cin, membership);

    string sql = "INSERT INTO members (name,email,phone,address,membershiptype,joindate,status) "
                 "VALUES ('" + name + "','" + email + "','" + phone + "','" + address + "','" + membership + "',GETDATE(),'Active')";
    cout << (execSQL(sql) ? "Member added.\n" : "Failed to add member.\n");
}

void updateMember() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string email;
    cout << "Enter email of member to update: "; getline(cin, email);
    if (!exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Member not found.\n"; 
        return;
    }

    string phone, address, membership;
    cout << "New phone (leave blank to skip): "; getline(cin, phone);
    cout << "New address (leave blank to skip): "; getline(cin, address);
    cout << "New Membership Type (Regular/Premium, leave blank to skip): "; getline(cin, membership);

    string sql = "UPDATE members SET ";
    bool first = true;

    if (!phone.empty()) { sql += (first ? "" : ", ") + string("Phone=N'") + phone + "'"; first=false; }
    if (!address.empty()) { sql += (first ? "" : ", ") + string("Address=N'") + address + "'"; first=false; }
    if (!membership.empty()) { sql += (first ? "" : ", ") + string("MembershipType=N'") + membership + "'"; }

    sql += " WHERE Email='" + email + "'";
    cout << (execSQL(sql) ? "Member updated successfully.\n" : "Failed to update member.\n");
}
void deleteMember() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string email;
    cout << "Enter email of member to deactivate: "; getline(cin, email);
    if (!exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Member not found.\n"; 
        return;
    }

    string sql = "UPDATE members SET Status=N'Inactive' WHERE Email='" + email + "'";
    cout << (execSQL(sql) ? "Member deactivated successfully.\n" : "Failed to deactivate member.\n");
}

void viewMembers() {
    string sql = "SELECT MemberID, Name, Email, Phone, Address, MembershipType, JoinDate, Status FROM members";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\tEmail\tPhone\tAddress\tMembership\tJoinDate\tStatus\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char name[150], email[100], phone[20], address[255], membership[20], status[20];
            char joinDate[20];  // format YYYY-MM-DD

            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, phone, sizeof(phone), NULL);
            SQLGetData(stmt, 5, SQL_C_CHAR, address, sizeof(address), NULL);
            SQLGetData(stmt, 6, SQL_C_CHAR, membership, sizeof(membership), NULL);
            SQLGetData(stmt, 7, SQL_C_CHAR, joinDate, sizeof(joinDate), NULL);
            SQLGetData(stmt, 8, SQL_C_CHAR, status, sizeof(status), NULL);

            cout << id << "\t" << name << "\t" << email << "\t" << phone << "\t" << address << "\t"
                 << membership << "\t" << joinDate << "\t" << status << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void searchMembers() {
    string keyword;
    cout << "Enter name/email to search: ";
    getline(cin, keyword);

    string sql = "SELECT MemberID, Name, Email, Phone, Address, MembershipType, JoinDate, Status FROM members "
                 "WHERE Name LIKE N'%" + keyword + "%' OR Email LIKE N'%" + keyword + "%'";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\tEmail\tPhone\tAddress\tMembership\tJoinDate\tStatus\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char name[150], email[100], phone[20], address[255], membership[20], status[20];
            char joinDate[20];

            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, phone, sizeof(phone), NULL);
            SQLGetData(stmt, 5, SQL_C_CHAR, address, sizeof(address), NULL);
            SQLGetData(stmt, 6, SQL_C_CHAR, membership, sizeof(membership), NULL);
            SQLGetData(stmt, 7, SQL_C_CHAR, joinDate, sizeof(joinDate), NULL);
            SQLGetData(stmt, 8, SQL_C_CHAR, status, sizeof(status), NULL);

            cout << id << "\t" << name << "\t" << email << "\t" << phone << "\t" << address << "\t"
                 << membership << "\t" << joinDate << "\t" << status << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
// ... similar functions for updateMember, deleteMember, viewMembers, searchMembers
void issueBook() {
    int memberId, bookId;
    cout << "Enter Member ID: ";
    cin >> memberId;
    cout << "Enter Book ID: ";
    cin >> bookId;
    cin.ignore();

    // Check book availability
    string checkBookSQL = "SELECT Availability FROM Books WHERE BookID = " + to_string(bookId);
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLExecDirect(stmt, (SQLCHAR*)checkBookSQL.c_str(), SQL_NTS);

    int available = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS)
        SQLGetData(stmt, 1, SQL_C_LONG, &available, 0, NULL);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (!available) {
        cout << "Book is not available.\n";
        return;
    }

    // Max 5 books per member
    string checkLimitSQL = "SELECT COUNT(*) FROM Transactions WHERE MemberID = " + to_string(memberId) + " AND ReturnDate IS NULL";
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLExecDirect(stmt, (SQLCHAR*)checkLimitSQL.c_str(), SQL_NTS);

    int count = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS)
        SQLGetData(stmt, 1, SQL_C_LONG, &count, 0, NULL);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (count >= 5) {
        cout << "Member has reached max limit (5 books).\n";
        return;
    }

    string sql = "INSERT INTO Transactions (MemberID, BookID, IssueDate, DueDate) "
                 "VALUES (" + to_string(memberId) + "," + to_string(bookId) + ", GETDATE(), DATEADD(day,14,GETDATE()))";
    if (execSQL(sql)) {
        execSQL("UPDATE Books SET Availability = 0 WHERE BookID = " + to_string(bookId));
        cout << "Book issued successfully.\n";
    } else {
        cout << "Failed to issue book.\n";
    }
}
void returnBook() {
    int bookId;
    cout << "Enter Book ID to return: ";
    cin >> bookId;
    cin.ignore();

    string sql = "SELECT TransactionID, DATEDIFF(day, DueDate, GETDATE()) AS LateDays "
                 "FROM Transactions WHERE BookID = " + to_string(bookId) + " AND ReturnDate IS NULL";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        cout << "Error retrieving transaction.\n";
        return;
    }

    int txId = 0, lateDays = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_SLONG, &txId, 0, NULL);
        SQLGetData(stmt, 2, SQL_C_SLONG, &lateDays, 0, NULL);
    } else {
        cout << "No active transaction found.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    double fine = lateDays > 0 ? lateDays * 5.0 : 0.0;
    string updateSQL = "UPDATE Transactions SET ReturnDate = GETDATE(), FineAmount = " + to_string(fine) +
                       " WHERE TransactionID = " + to_string(txId);
    execSQL(updateSQL);
    execSQL("UPDATE Books SET Availability = 1 WHERE BookID = " + to_string(bookId));
    cout << "Book returned. Fine: ₹" << fine << "\n";
}