#include <curses.h>
#include <iostream>
#include <sstream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

#include <boost/thread.hpp>

#include "../include/connectionHandler.h"
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

// trim from start
static inline std::string &ltrim(std::string &s) {
     s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
     return s;
 }

// trim from end
static inline std::string &rtrim(std::string &s) {
     s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
     return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
     return ltrim(rtrim(s));
}

using namespace std;

class User;
class Channel;
class Message;
class Window;

typedef std::vector<std::string> Strings;
typedef std::vector<Window*> Windows;
typedef std::vector<User*> Users;
typedef std::vector<Channel*> Channels;

/**
 * Wordwrap a @param str (insert \n) every @param width characters.
 * @param std::string str String to wordwrap.
 * @param size_t width    Width to wordwrap it to.
 * @return std::string Wordwrapped string.
**/
std::string wordwrap(const std::string& str, size_t width) {
    std::string newString(str);
    for (size_t ii = width; ii < newString.size(); ii += width) {
        newString.insert(ii, "\n");
    }

    return newString;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

class User {
    public:
        User(std::string nick) :
            _nick(nick),
            _name(nick),
            _chanMode(""),
            _channels()
        {
            // detect channel modes in the nick and set them 
            // accordingly.
            if (nick.at(0) == '@' || nick.at(0) == '+') {
                this->_chanMode = this->_nick.at(0);
                this->_nick = this->_nick.substr(1);
            }
        };

        User(User& other) :
            _nick(other.getNick()),
            _name(other.getName()),
            _chanMode(other.getChanMode()),
            _channels(other.getChannels())
        {           
        };

        void addChannel(Channel* channel) {
            _channels.push_back(channel);
        };

        void removeChannel(Channel* channel) {
            Channels::iterator position = std::find(
                                                _channels.begin(), 
                                                _channels.end(),
                                                channel
                                                );
            if (position != _channels.end()) {
                _channels.erase(position);
            }
        };
        
        bool isInChannel(Channel* channel) {
            Channels::iterator position = std::find(
                                                _channels.begin(), 
                                                _channels.end(),
                                                channel
                                                );
            return (position != _channels.end());
        };

        void setNick(std::string nick) {
            this->_nick = nick;
        };

        void setName(std::string name) {
            this->_name = name;
        };

        std::string getName() const {
            return this->_name;
        };

        std::string getNick() const {
            return this->_nick;
        };

        Channels getChannels() const {
            return this->_channels;
        };

        std::string toString() const {
            return this->getFullNick();
        };

        std::string getChanMode() const {
            return this->_chanMode;
        };

        /**
         * Return nick containing channel mode.
        **/
        std::string getFullNick() const {
            return string(this->getChanMode()).append(this->getNick());
        };
    
        bool operator <(const User& rhs) const {
            return this->getFullNick() < rhs.getFullNick();
        }

    private:
        std::string _nick;
        std::string _name;
        std::string _chanMode;
        Channels _channels;
};

bool UserPointerCompare (const User* l, const User* r) {
    return *l < *r;
};

class Channel {
    public:
        Channel(std::string name) : 
            _name(name), 
            _topic(""), 
            _users() 
        {
        };

        virtual ~Channel() {
        };

        void setTopic(std::string topic) {
            this->_topic = topic;  
        };

        void addUser(User* user) {
            this->_users.push_back(user);
        };

        /**
         * Add a list of users.
        **/
        virtual void addUsers(Users users) {
            for (Users::iterator it = users.begin();
                 it != users.end();
                 ++it)
            {
                this->addUser(*it);
            }
        };

        void removeUser(User* user) {
            Users::iterator position = std::find(
                                            this->_users.begin(), 
                                            this->_users.end(),
                                            user
                                        );

            if (position != _users.end()) {
                this->_users.erase(position);
            }
        };

        size_t getUsersCount() const {
            return this->_users.size();
        };

        std::string getName() const {
            return this->_name;
        };

        std::string getTopic() const {
            return this->_topic;
        };

        Users getUsers() const {
            return this->_users;
        };

        std::string toString() const {
            return string()
                    .append(getName())
                    .append(" - ")
                    .append(getTopic())
                  ;
        };

    private:
        std::string _name;
        std::string _topic;
        Users _users;
};

class Message {
    public:
        enum Type { DEFAULT, PRIVATE, ACTION, SYSTEM, DEBUG };

        Message(std::string text) :
            _user(NULL),
            _text(text),
            _nick(""),
            _timestamp(time(0)),
            _type(Message::DEFAULT)
        {
        };

        Message(std::string text, User* user) :
            _user(user),
            _text(text),
            _nick(user->getFullNick()),
            _timestamp(time(0)),
            _type(Message::DEFAULT)
        {
        };
        
        Message(std::string text, Message::Type type) :
            _user(NULL),
            _text(text),
            _nick(""),
            _timestamp(time(0)),
            _type(type)
        {
        };

        Message(std::string text, User* user, Message::Type type) :
            _user(user),
            _text(text),
            _nick(user->getFullNick()),
            _timestamp(time(0)),
            _type(type)
        {
        };
        Message (Message& other) :
            _user(new User(*other.getUser())),
            _text(other.getText()),
            _nick(other.getNick()),
            _timestamp(other.getTimestamp()),
            _type(Message::DEFAULT)
        {
        };
        
        Message operator=(const Message& other) {
            if (this == &other) {
                return *this;
            }

            this->_user = new User(*other.getUser());
            this->_text = other.getText();
            this->_nick = other.getNick();
            this->_type = other.getType();
            return *this;
        };

        User* getUser() const {
            return _user;
        };

        std::string getText() const {
            return _text;
        };

        std::string getNick() const {
            return _nick;
        };

        short getType() const {
            return _type;
        };

        time_t getTimestamp() const {
            return _timestamp;
        };

        std::string getTimestampString() const {
            struct tm* now = localtime(&_timestamp);
            ostringstream time;
            time << std::setfill('0') << std::setw(2) << (1 + now->tm_hour) 
                 << ":" 
                 << std::setfill('0') << std::setw(2) << now->tm_min;
            return time.str();
        };

        std::string toString() const {
            std::string str;
             
            str.append(getTimestampString()).append(" ");

            if (Message::PRIVATE == this->getType()) {
                str.append("<= ").append(this->getNick()).append(": ");
            } else if (Message::SYSTEM == this->getType()) {
                str.append("*** ");
            } else if (Message::DEBUG == this->getType()) {
                str.append("==debug==  ");
            } else if (Message::ACTION == this->getType()) {
                str.append("* ").append(this->getNick()).append(" ");
            } else {
                str.append("<").append(this->getNick()).append("> ");
            }

            str.append(this->getText());

            return str;
        };

    private:
        User* _user; // message associated user
        std::string _text; // message text
        std::string _nick; // associated nick
        time_t _timestamp; // timestamp of message arrival
        short _type; // type of message 
};

class Window {
    public:
        Window(std::string name, 
               int height, 
               int width, 
               int starty, 
               int startx) :
            _win(newwin(height, width, starty, startx)),
            _name(name),
            _height(height),
            _width(width),
            _starty(starty),
            _startx(startx),
            _refreshAfter()
        {
            // allow detections of special keys
            keypad(_win, TRUE);

            this->setup();

            this->refreshWindow();
        };

        virtual void setup() {
            // draw window borders
            wborder(_win, '|', '|', '-', '-', '+', '+', '+', '+');
        };

        virtual void refreshWindow() {
            wrefresh(_win);
            this->refreshAfter();    
        };

        virtual void print(std::string text) {
            wprintw(_win, text.c_str());
            this->refreshAfter();    
        };

        virtual void print(int y, int x, std::string text) {
            mvwprintw(_win, y, x, text.c_str());
            this->refreshAfter();    
        };

        /**
         * Refresh all windows in _refreshAfter vector.
        **/
        virtual void refreshAfter() {
            for (Windows::iterator it = _refreshAfter.begin();
                 it != _refreshAfter.end(); 
                 ++it)
            {
                (*it)->refreshWindow();
            }
        };

        virtual void addRefreshAfterWindow(Window* window) {
            _refreshAfter.push_back(window);
        };

        virtual void clear() {
            wclear(_win);
            this->setup();
            this->refreshWindow();
        };

        virtual ~Window() {
            delwin(_win);
        };

    protected:
        WINDOW* _win;
        std::string _name;
        int _height;
        int _width;
        int _starty;
        int _startx;
        Windows _refreshAfter;
};

class InputWindow : public Window {
    public:
        InputWindow(std::string name, 
               int height, 
               int width, 
               int starty, 
               int startx) :
            Window(name, height, width, starty, startx),
            _inputY(1),
            _inputX(1),
            _input()
        {
        }

        virtual void redraw() {
            this->clear();
            this->print(_inputY, _inputX, this->getInput());
        };

        virtual void setInputY(int inputY) {
            _inputY = inputY;
        };

        virtual void setInputX(int inputX) {
            _inputX = inputX;
        };

        virtual void setInputYX(int inputY, int inputX) {
            _inputY = inputY;
            _inputX = inputX;
        };

        virtual int getChar() {
            return mvwgetch(_win, _inputY, _inputX + _input.str().size());
        };

        virtual void putChar(char ch) {
            _input << (char)ch;
            this->redraw();
        };

        virtual void deleteLastChar() {
            std::string trimedStr = _input.str().substr(0, _input.str().size()-1);
            _input.str("");
            _input << trimedStr;
            this->redraw();
        };
        
        virtual void clearInput() {
            _input.str("");
            _input.clear();
            this->clear();
        };

        virtual std::string getInput() {
            return _input.str();
        };

        virtual std::string str() {
            return _input.str();
        };

    protected:
        int _inputY;
        int _inputX;

        ostringstream _input;
};

template <typename T>
class ContentWindow : public Window {
    public:
        ContentWindow(
            std::string name, 
            int height, 
            int width, 
            int starty, 
            int startx
        ) :
            Window(name, height, width, starty, startx),
            _content(""),
            _contentElement(NULL),
            _offsetY(1),
            _offsetX(1)
        {
        }

        virtual void redraw() {
            this->clear();
            
            this->print(_offsetY, _offsetY, this->getContent());
            this->refreshWindow();
        };
        
        virtual void setOffsetY(int offsetY) {
            _offsetY = offsetY;
        };

        virtual void setOffsetX(int offsetX) {
            _offsetX = offsetX;
        };

        virtual void setOffsetYX(int offsetY, int offsetX) {
            _offsetY = offsetY;
            _offsetX = offsetX;
        };

        virtual size_t getOffsetY() const {
            return _offsetY;
        };

        virtual size_t getOffsetX() const {
            return _offsetX;
        };

        virtual void setContent(T t) {
            _contentElement = t;

            this->redraw();
        };

        virtual void setContent(std::string content) {
            _content = content;

            this->redraw();
        };

        virtual void appendContent(std::string content) {
            _content += content;
            
            this->redraw();
        };

        virtual void appendContent(char ch) {
            ostringstream ss;
            ss << (char)ch;

            this->appendContent(ss.str());
        };
        
        virtual std::string getContent() const {
            if (NULL == _contentElement) {
                return _content;
            }

            return _contentElement->toString();
        };
        
        virtual void clearContent() {
            _content.clear();
            this->clear();
        };

    protected:
        std::string _content;
        T _contentElement;
        int _offsetY;
        int _offsetX;
};

template <typename T>
class ListWindow : public ContentWindow<T> {
    public:
        ListWindow(
            std::string name, 
            int height, 
            int width, 
            int starty, 
            int startx
        ) :
            ContentWindow<T>(name, height, width, starty, startx),
            _list(),
            _visibleSize(-1),
            _reverseList(true)
        {
        }

        virtual void redraw() {
            this->clear();
            
            // count, from the bottom up, how many lines each item string
            // representation will require. get up to _visibleSize lines
            // and then spit them out at once.
            
            std::vector<std::string> outputLines;
            size_t linesCount = 0;

            if (_reverseList) {
                for (typename std::vector<T>::reverse_iterator it = _list.rbegin();
                     it != _list.rend() && linesCount < _visibleSize;
                     ++it)
                {
                    // wordwrap the text at _wrapWidth 
                    // and then split it to lines.
                    Strings lines = split(wordwrap((*it)->toString(), 150), '\n');

                    // reverse iterate on the lines
                    // and add them to the output lines.
                    for (Strings::reverse_iterator jj = lines.rbegin();
                         jj != lines.rend() && linesCount < _visibleSize;
                         ++jj) 
                    {
                        outputLines.push_back(*jj); 
                        linesCount++;
                    }
                }

                int ii = 0;
                for (Strings::reverse_iterator r_it = outputLines.rbegin();
                     r_it != outputLines.rend();
                     ++r_it, ++ii)
                {
                    this->print(this->getOffsetY() + ii, this->getOffsetX(), (*r_it));
                } 
            } else {
                for (typename std::vector<T>::iterator it = _list.begin();
                     it != _list.end() && linesCount < _visibleSize;
                     ++it)
                {
                    // wordwrap the text at _wrapWidth 
                    // and then split it to lines.
                    Strings lines = split(wordwrap((*it)->toString(), 150), '\n');

                    // iterate on the lines
                    // and add them to the output lines.
                    for (Strings::iterator jj = lines.begin();
                         jj != lines.end() && linesCount < _visibleSize;
                         ++jj) 
                    {
                        outputLines.push_back(*jj); 
                        linesCount++;
                    }
                }

                int ii = 0;
                for (Strings::iterator it = outputLines.begin();
                     it != outputLines.end();
                     ++it, ++ii)
                {
                    this->print(this->getOffsetY() + ii, this->getOffsetX(), (*it));
                } 
            }

            this->refreshWindow();
        };

        /**
         * Add an item to the list.
        **/
        virtual void addItem(T item) {
            _list.push_back(item);
            
            this->redraw();
        };

        /**
         * Add a list of items.
        **/
        virtual void addItems(std::vector<T> items) {
            for (typename std::vector<T>::iterator it = items.begin();
                 it != items.end();
                 ++it)
            {
                _list.push_back(*it);
            }

            this->redraw();
        };

        virtual void setItem(int index, T item) {
            _list.at(index) = item;
            
            this->redraw();
        };

        virtual void removeItem(int index) {
            _list.erase(_list.begin() + index);

            this->redraw();
        };

        virtual void removeAll() {
            _list.clear();

            this->redraw();
        };

        virtual std::vector<T> getList() {
            return _list;
        };

        virtual void setVisibleSize(size_t size) {
            _visibleSize = size;
        };

        virtual void setReverseList(bool reverseList) {
            _reverseList = reverseList;
        };  

        virtual size_t size() {
            return _list.size();
        };
        
    protected:
        std::vector<T> _list;
        size_t _visibleSize;
        bool _reverseList;
};

template <typename T>
bool from_string(T& t,
                const std::string& s,
                std::ios_base& (*f)(std::ios_base&) = std::dec)
{
    std::istringstream iss(s);
    return !(iss >> f >> t).fail();
}

class UI {
    public:
        UI(ContentWindow<Channel*>* wTitle, 
           ListWindow<Message*>* wHistory, 
           ListWindow<User*>* wNames, 
           InputWindow* wInput) :
            title(wTitle),
            history(wHistory),
            names(wNames),
            input(wInput),
            _hasStartedNamesStream(false)
        {
        }

        virtual void startNamesStream() {
            if (false == _hasStartedNamesStream) {
                _hasStartedNamesStream = true;
            }
        };

        virtual void endNamesStream() {
            _hasStartedNamesStream = false;

            if (_namesStream.size() > 0) {
                this->names->removeAll();

                Users users;
                for (Strings::iterator it = _namesStream.begin();
                     it != _namesStream.end();
                     ++it)
                {
                    User* newUser = new User(*it);
                    users.push_back(newUser);
                }

                sort(users.begin(), users.end(), UserPointerCompare);
                this->addUsers(users);
                _namesStream.clear();
            }
        };

        virtual void addNames(std::string str) {
            Strings names = split(str, ' ');
            for (Strings::iterator it = names.begin();
                 it != names.end();
                 ++it)
            {
                _namesStream.push_back(*it);
            }
        };

        virtual Channel* getChannel() {
            return _channel;
        };

        virtual void setChannel(Channel* newChannel) {
            _channel = newChannel;
            this->title->setContent(_channel);
            this->names->removeAll();
        };

        virtual void addUser(User* newUser) {
            this->names->addItem(newUser);

            if (NULL != this->getChannel()) {
                this->getChannel()->addUser(newUser);
            }
        };

        virtual void addUsers(Users users) {
            this->names->addItems(users);
            
            if (NULL != this->getChannel()) {
                this->getChannel()->addUsers(users);
            }
        };

        ContentWindow<Channel*>* title;
        ListWindow<Message*>* history;
        ListWindow<User*>* names;
        InputWindow* input;

    private:
        bool _hasStartedNamesStream;
        Strings _namesStream;
        Channel* _channel;
};

bool _debug = false;
void debug (ListWindow<Message*>* history, std::string message) {
    if (!_debug) {
        return;
    }

    history->addItem(new Message(message, new User("debug"), Message::DEBUG));
};

class Network {
    public:
        Network(ConnectionHandler* ch) : 
            _ch(ch)
        {
        }

        void read(UI* ui, User* user) { 
            std::string answer;
            while (_ch->read(answer)) {
                answer = trim(answer);
                debug(ui->history, answer);
                
                if (answer.at(0) == ':') {
                    // this is a command!
                    Strings data = split(answer.substr(1), ':');
                    Strings params = split(data[0], ' ');
                    std::string host = params[0];
                    std::string command = params[1]; 
                        
                    std::string target;
                    if (params.size() > 2) {
                        target = params[2]; 
                    }

                    std::string msg;
                    if (data.size() > 1) {
                        // find the second occurrence of ':'
                        // and fetch the whole string AFTER it
                        msg = answer.substr(answer.find(':', 1)+1);
                    }
                
                    // if the one who commited the action is a user,
                    // his host will be: nick!user@host
                    std::string nick = host.substr(0, host.find('!'));

                    if (command == "JOIN") {
                        if (nick == user->getNick()) {
                            // joined a new channel!
                            ui->setChannel(new Channel(target));
                            
                            ui->history->addItem(
                                new Message(
                                    string("Now talking on ").append(target),
                                    Message::SYSTEM
                                )
                            );
                        } else {
                            // someone else joined a channel we are in
                            // add him to the names list.
                            User* newUser = new User(nick);
                            ui->addUser(newUser);
                            ui->history->addItem(
                                new Message(
                                    string("has joined ").append(target),
                                    newUser,
                                    Message::ACTION
                                )
                            );
                        }

                    } else if (command == "PART") {
                        if (nick == user->getNick()) {
                            // quit channel. 
                            ui->setChannel(NULL);
                            
                            ui->history->addItem(
                                new Message(
                                    string("You have left ").append(target),
                                    Message::SYSTEM
                                )
                            );
                        } else {
                            // someone quit the current channel.
                            // find him on the names list and remove him.
                            Users users = ui->names->getList();
                            for (size_t ii = 0; ii < users.size(); ++ii) {
                                if (nick == users[ii]->getNick()) {
                                    // found him! remove from the list
                                    ui->names->removeItem(ii);
                                    
                                    std::string text = "has left ";
                                    text.append(target);
                                    if (msg != "") {
                                        text.append(" (").append(msg).append(")");
                                    }
                                    ui->history->addItem(
                                        new Message(
                                            text,
                                            users[ii],
                                            Message::ACTION
                                        )
                                    );
                                    
                                    break;
                                }
                            }
                        }

                    } else if (command == "PRIVMSG") {
                        if (NULL != ui->getChannel() 
                            && target == ui->getChannel()->getName()) {
                            // message to current channel!
                            ui->history->addItem(
                                new Message(
                                    msg,
                                    new User(nick)
                                )
                            );
                        } else if (target == user->getNick()) {
                            // a private message to the user
                            ui->history->addItem(
                                new Message(
                                    msg,
                                    new User(nick),
                                    Message::PRIVATE 
                                )
                            );
                        }

                    } else if (command == "QUIT") {
                        // someone quit while being in the current channel.
                        // find him on the names list and remove him.
                        Users users = ui->names->getList();
                        for (size_t ii = 0; ii < users.size(); ++ii) {
                            if (nick == users[ii]->getNick()) {
                                // found him! remove from the list
                                ui->names->removeItem(ii);

                                std::string text = "has quit";
                                if (msg != "") {
                                    text.append(" (").append(msg).append(")");
                                }
                                ui->history->addItem(
                                    new Message(
                                        text,
                                        users[ii],
                                        Message::ACTION
                                    )
                                );
                            
                                break;
                            }
                        }
                    } else if (command == "NICK") {
                        if (nick == user->getNick()) {
                            // current user changed nick!
                            // update current user
                            user->setNick(msg);
                        } else {
                            // someone else changed nick.
                            // find him in channel list and update his nick.
                            Users users = ui->names->getList();
                            for (size_t ii = 0; ii < users.size(); ++ii) {
                                if (nick == users[ii]->getNick()) {
                                    // found the user. change his nick
                                    // to the new nick.
                                    users[ii]->setNick(msg);
                                    ui->names->redraw();
                                    
                                    ui->history->addItem(
                                        new Message(
                                            string("is now known as ")
                                                .append(msg),
                                            users[ii],
                                            Message::ACTION
                                        )
                                    );

                                    break;
                                }
                            }
                        }

                    } else if (command == "NOTICE") {
                        if (NULL != ui->getChannel() && 
                            target == ui->getChannel()->getName()) {
                            // message to current channel!
                            ui->history->addItem(
                                new Message(
                                    msg,
                                    new User(nick)
                                )
                            );
                        } else if (target == user->getNick()) {
                            // a private notice to the user
                            ui->history->addItem(
                                new Message(
                                    msg,
                                    new User(nick),
                                    Message::PRIVATE 
                                )
                            );
                        }

                    } else if (command == "332" || command == "TOPIC") {
                        // topic change
                        // changing topic 
                        ui->getChannel()->setTopic(msg); 
                        ui->title->redraw();

                        ui->history->addItem(
                            new Message(
                                string("has changed the topic to: ")
                                    .append(msg),
                                new User(nick),
                                Message::ACTION
                            )
                        );
                    } else if (command == "353") { // names list
                        // this will start a names stream 
                        // only if there is none active
                        ui->startNamesStream();
                        ui->addNames(msg);

                    } else if (command == "366") { // end names list
                        // stop streaming names to the ui.
                        // this will set the names list.
                        ui->endNamesStream();

                    } else if (command == "001"
                               || command == "002"
                               || command == "003"
                               || command == "004"
                               || command == "005"
                               || command == "251"
                               || command == "252"
                               || command == "254"
                               || command == "255"
                               || command == "372"
                               || command == "372"
                               || command == "375"
                               || command == "376"
                            ) {
                        ui->history->addItem(
                            new Message(
                                msg,
                                Message::SYSTEM
                            )
                        );
                    }
                } else {
                    // this is a command!
                    Strings params = split(answer, ':');
                    std::string command = trim(params[0]); 
                    
                    ostringstream response;
                    if (command == "PING") {
                        response << "PONG :" << params[1];
                        _ch->send(response.str());
                    } else if (command == "NOTICE AUTH") {
                        ui->history->addItem(
                            new Message(
                                params[1],
                                Message::SYSTEM
                            )
                        );
                    }
                }

                answer.clear();
            }
        };

    private:
        ConnectionHandler* _ch;
};

int main(int argc, char *argv[])
{
    
    if (argc != 4) {
        cout << "Usage: " << argv[0] << " host port nick" << std::endl;
        return 1;
    }

    string host(argv[1]);
    unsigned short port = atoi(argv[2]);
    string nick(argv[3]);

    std::cout << "Creating connection..." << std::endl;
    ConnectionHandler server(host, port);
    std::cout << "Connecting to server..." << std::endl;
    if (!server.connect()) {
        std::cout << "Cannot connect to " << host << ":" << port << std::endl;
        return 1;
    }
    
    User* user = new User(nick);

    if (!server.send(
                string("NICK ")
                .append(user->getNick())
                )
        ) 
    {
        std::cout << "Disconnected. Exiting..." << std::endl;
        return 0;
    }
   
    if (!server.send(
                string("USER ")
                .append(user->getNick())
                .append(" 0 * :")
                .append(user->getName())
            )
        ) 
    {
        std::cout << "Disconnected. Exiting..." << std::endl;
        return 0;
    }

    int ch = 0;

    initscr();          /* Start curses mode        */
    start_color();          /* Start the color functionality */
    cbreak();           /* Line buffering disabled, Pass on
                         * everty thing to me       */
    keypad(stdscr, TRUE);       /* I need that nifty F1     */
    noecho();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);

    assume_default_colors(COLOR_GREEN, COLOR_BLACK);
    
//    attron(COLOR_PAIR(1));
//    mvprintw(0, 0, "Press End to exit");
//    refresh();
//    attroff(COLOR_PAIR(1));
    

    InputWindow* wInput = new InputWindow("input", 3, 153, 45, 0);
    ListWindow<User*>* wNames = new ListWindow<User*>("names", 46, 17, 2, 152);
    ContentWindow<Channel*>* wTitle = new ContentWindow<Channel*>("title", 3, 169, 0, 0);
    ListWindow<Message*>* wHistory = new ListWindow<Message*>("main", 44, 153, 2, 0);
   
    // UI is a set of title, history, names and input windows.
    UI* ui = new UI(wTitle, wHistory, wNames, wInput);
    
    ui->names->addRefreshAfterWindow(wInput);
    ui->names->setVisibleSize(44);
    ui->names->setReverseList(false);
    ui->names->addItem(user);
    
    ui->title->addRefreshAfterWindow(wInput);

    ui->history->addRefreshAfterWindow(wInput);
    ui->history->setVisibleSize(42);

    debug(ui->history, "*** Starting... ***");

    Network networkRead(&server);
    boost::thread networkInputThread(&Network::read, &networkRead, ui, user);
    
    std::string line;
    do {
        switch(ch) {   
            case 0: // first run!
                // do nothing here for now.
                break;

            case 263: // backspace
                ui->input->deleteLastChar();
                break;

            case 10: // send message to server!
                line = ui->input->str();
                ui->input->clearInput(); 
                
                if (line.size() == 0) {
                    // empty message; don't do anything
                    break;
                }
                
                if (line.at(0) == '/') {
                    // this is a command!
                    std::string command = line.substr(1, line.find(' ')-1);
                    std::string params;
                    if (line.find(' ') != std::string::npos) {
                        params = line.substr(line.find(' ')+1);
                    }
                    
                    if (command == "nick") {
                        // changing nick.
                        server.send(string("NICK ").append(params));
                    } else if (command == "join") {
                        server.send(string("JOIN ").append(params));
                    } else if (command == "part") {
                        if (params == "") {
                            // part current channel
                            // check if we are already in a channel
                            if (NULL == ui->getChannel()) {
                                // no channel! 
                                // alert the user that he should join first
                                ui->history->addItem(
                                    new Message("Can't send message - join a channel first!")
                                );

                                break;
                            }
                            
                            server.send(string("PART ").append(ui->getChannel()->getName()));
                        } else {
                            server.send(string("PART ").append(params));
                        }
                        
                    } else if (command == "topic") {
                        // check if we are already in a channel
                        if (NULL == ui->getChannel()) {
                            // no channel! 
                            // alert the user that he should join first
                            ui->history->addItem(
                                new Message("Can't send message - join a channel first!")
                            );

                            break;
                        }
                        
                        server.send(
                            string("TOPIC ")
                            .append(ui->getChannel()->getName())
                            .append(" :")
                            .append(params)
                        );
                    } else if (command == "quit") {
                        server.send(string("QUIT :").append(params));
                    } else if (command == "clear") {
                        ui->history->removeAll();
                    } else if (command == "raw") { // allow sending raw messages to server
                        server.send(params);
                    } else if (command == "debug") { // toggle debug messages
                        _debug = !_debug;
                    }
                } else {
                    // this is a message.
                    // check if we are already in a channel
                    if (NULL == ui->getChannel()) {
                        // no channel! 
                        // alert the user that he should join first
                        ui->history->addItem(
                            new Message("Can't send message - join a channel first!")
                        );

                        break;
                    }
                    
                    // put it in the channel history list
                    Message* message = new Message(line, user);
                    ui->history->addItem(message);

                    // transmit message to the server
                    server.send(
                        string("PRIVMSG ")
                        .append(ui->getChannel()->getName())
                        .append(" :")
                        .append(line)
                    );
                }
                break;
            
            default:
                ui->input->putChar(ch);
                break;
        } 
    } while((ch = ui->input->getChar()) != KEY_END);

    networkInputThread.join();
    
    // end curses mode
    endwin();

    return 0;
}