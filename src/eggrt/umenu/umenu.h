/* umenu.h
 * Egg Universal Menu.
 * Press something during play, and we open a universal menu.
 * I'm thinking use AUX3 for this, and no longer expose it to clients. TODO Decide.
 * We are essentially an egg client game. Access to the platform api, etc.
 * ** We do use eggrt globals. **
 */
 
#ifndef UMENU_H
#define UMENU_H

struct umenu;

void umenu_del(struct umenu *umenu);

struct umenu *umenu_new();

int umenu_update(struct umenu *umenu,double elapsed);
int umenu_render(struct umenu *umenu);

#endif
