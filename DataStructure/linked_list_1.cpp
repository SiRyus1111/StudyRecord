struct Node{
    int data;
    Node* next;

    Node(int element) : data(element), next(nullptr) { };
};

class SLList{
private:
    Node* head;

public:
    SLList() : head(nullptr){ }
    
    Node* find(int element);

    void insert_front(int element);
    bool insert_node(int target, int element);

    bool delete_front(int target);
    bool delete_node(int target);
};

Node* SLList::find(int element){
    Node* current = head;
    while (current != nullptr) {
        if (current->data == element){
            return current;
        }

        current = current->next;
    }

    return nullptr;
}

void SLList::insert_front(int element){
    Node* new_node = new Node(element);

    new_node->next = head;
    head = new_node;
}

bool SLList::insert_node(int target, int element) {
    Node* previous = find(target);

    if (previous == nullptr) {
        return false;
    }

    Node* new_node = new Node(element);

    new_node->next = previous->next;
    previous->next = new_node;

    return true;
}

bool SLList::delete_front(int target){
    if (head == nullptr) {
        return false;
    }


    Node* current = head;
    head = current->next;
    delete current;

    return true;
}

bool SLList::delete_node(int target) {
    if (head == nullptr) {
        return false;
    }

    Node* previous = head;
    Node* current = head->next;

    if (head->data == target) {
        head = current;
        delete previous;
    }

    while (current != nullptr) {
        if (current->data == target) {
            previous->next = current->next;
            delete current;
            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}