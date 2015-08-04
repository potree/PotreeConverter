// 
// Classes for keeping track of the least recently used (LRU) items
// 


#ifndef LRU_H
#define LRU_H

namespace Potree{

template<class T>
class LRUItem{
public:
	LRUItem<T> *previous = NULL;
	LRUItem<T> *next = NULL;
	T element = NULL;

	LRUItem(T element){
		this->element = element;
	}
};

template<class T>
class LRU{

public:

	LRUItem<T> *first = NULL;
	LRUItem<T> *last = NULL;
	unordered_map<T, LRUItem<T>*> elements;

	void touch(T element){

		//cout << element << endl;
		
		auto it = elements.find(element);
		if(it == elements.end()){
			// add new to list
			LRUItem<T> *item = new LRUItem<T>(element);
			item->previous = this->last;
			this->last = item;
			if(item->previous != NULL){
				item->previous->next = item;
			}

			elements.insert({element, item});

			if(first == NULL){
				first = item;
			}
		}else{
			// update existing in list
			LRUItem<T> *item = it->second;

			if(item->previous == NULL){
				// handle touch on first element
				if(item->next != NULL){
					first = item->next;
					first->previous = NULL;
					item->previous = last;
					item->next = NULL;
					last = item;
					item->previous->next = item;
				}
			}else if(item->next == NULL){
				// handle touch on last element
			}else{
				// handle touch on inner elements
				item->previous->next = item->next;
				item->next->previous = item->previous;
				item->previous = last;
				item->next = NULL;
				last = item;
				item->previous->next = item;
			}

		}
	}

};

}

#endif