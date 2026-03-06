#include <linux/gfp.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "sepolicy.h"
#include "../klog.h" // IWYU pragma: keep

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)

#include "ss/symtab.h"

#define KSU_SUPPORT_ADD_TYPE

//////////////////////////////////////////////////////
// Declaration
//////////////////////////////////////////////////////

static struct avtab_node *get_avtab_node(struct policydb *db,
                                         struct avtab_key *key,
                                         struct avtab_extended_perms *xperms);

static bool add_rule(struct policydb *db, const char *s, const char *t,
                     const char *c, const char *p, int effect, bool invert);

static void add_rule_raw(struct policydb *db, struct type_datum *src,
                         struct type_datum *tgt, struct class_datum *cls,
                         struct perm_datum *perm, int effect, bool invert);

static void add_xperm_rule_raw(struct policydb *db, struct type_datum *src,
                               struct type_datum *tgt, struct class_datum *cls,
                               uint16_t low, uint16_t high, int effect,
                               bool invert);
static bool add_xperm_rule(struct policydb *db, const char *s, const char *t,
                           const char *c, const char *range, int effect,
                           bool invert);

static bool add_type_rule(struct policydb *db, const char *s, const char *t,
                          const char *c, const char *d, int effect);

static bool add_filename_trans(struct policydb *db, const char *s,
                               const char *t, const char *c, const char *d,
                               const char *o);

static bool add_genfscon(struct policydb *db, const char *fs_name,
                         const char *path, const char *context);

static bool add_type(struct policydb *db, const char *type_name, bool attr);

static bool set_type_state(struct policydb *db, const char *type_name,
                           bool permissive);

static void add_typeattribute_raw(struct policydb *db, struct type_datum *type,
                                  struct type_datum *attr);

//////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////

static struct avtab_node *get_avtab_node(struct policydb *db,
                                         struct avtab_key *key,
                                         struct avtab_extended_perms *xperms)
{
    struct avtab_node *node;

    for (node = avtab_search_node(&db->te_avtab, key); node;
         node = avtab_search_node_next(node, key->specified)) {
        if (xperms && node->datum.u.xperms &&
            memcmp(node->datum.u.xperms, xperms,
                   sizeof(struct avtab_extended_perms)) == 0)
            return node;
        if (!xperms && !node->datum.u.xperms)
            return node;
    }

    node =
        ksu_kvzalloc(sizeof(struct avtab_node), GFP_ATOMIC);
    if (!node)
        return NULL;

    node->key = *key;
    if (xperms) {
        node->datum.u.xperms =
            ksu_kvzalloc(sizeof(struct avtab_extended_perms), GFP_ATOMIC);
        if (!node->datum.u.xperms) {
            ksu_kvfree(node);
            return NULL;
        }
        *node->datum.u.xperms = *xperms;
    }

    if (avtab_insert_node(&db->te_avtab, node)) {
        if (xperms)
            ksu_kvfree(node->datum.u.xperms);
        ksu_kvfree(node);
        return NULL;
    }

    return node;
}

static void add_rule_raw(struct policydb *db, struct type_datum *src,
                         struct type_datum *tgt, struct class_datum *cls,
                         struct perm_datum *perm, int effect, bool invert)
{
    struct avtab_key key;
    struct avtab_node *node;

    key.source_type = src->value;
    key.target_type = tgt->value;
    key.target_class = cls->value;
    key.specified = effect;

    node = get_avtab_node(db, &key, NULL);
    if (!node)
        return;

    if (invert)
        node->datum.u.data &= ~(1U << (perm->value - 1));
    else
        node->datum.u.data |= 1U << (perm->value - 1);
}

static bool add_rule(struct policydb *db, const char *s, const char *t,
                     const char *c, const char *p, int effect, bool invert)
{
    struct type_datum *src, *tgt;
    struct class_datum *cls;
    struct perm_datum *perm;

    src = hashtab_search(db->p_types.table, (char *)s);
    if (!src)
        return false;

    tgt = hashtab_search(db->p_types.table, (char *)t);
    if (!tgt)
        return false;

    cls = hashtab_search(db->p_classes.table, (char *)c);
    if (!cls)
        return false;

    perm = hashtab_search(cls->permissions.table, (char *)p);
    if (!perm && cls->comdatum)
        perm = hashtab_search(cls->comdatum->permissions.table, (char *)p);
    if (!perm)
        return false;

    add_rule_raw(db, src, tgt, cls, perm, effect, invert);

    return true;
}

static void add_xperm_rule_raw(struct policydb *db, struct type_datum *src,
                               struct type_datum *tgt, struct class_datum *cls,
                               uint16_t low, uint16_t high, int effect,
                               bool invert)
{
    struct avtab_key key;
    struct avtab_node *node;
    struct avtab_extended_perms xperms;
    unsigned int i;

    key.source_type = src->value;
    key.target_type = tgt->value;
    key.target_class = cls->value;
    key.specified = effect;

    memset(&xperms, 0, sizeof(xperms));
    xperms.specified = AVTAB_XPERMS_IOCTLS;
    xperms.driver = low >> 8;

    node = get_avtab_node(db, &key, &xperms);
    if (!node)
        return;

    for (i = low; i <= high; i++) {
        if (invert)
            node->datum.u.xperms->perms[i >> 5] &= ~(1U << (i & 0x1f));
        else
            node->datum.u.xperms->perms[i >> 5] |= 1U << (i & 0x1f);
    }
}

static bool add_xperm_rule(struct policydb *db, const char *s, const char *t,
                           const char *c, const char *range, int effect,
                           bool invert)
{
    struct type_datum *src, *tgt;
    struct class_datum *cls;
    uint16_t low, high;

    src = hashtab_search(db->p_types.table, (char *)s);
    if (!src)
        return false;

    tgt = hashtab_search(db->p_types.table, (char *)t);
    if (!tgt)
        return false;

    cls = hashtab_search(db->p_classes.table, (char *)c);
    if (!cls)
        return false;

    if (sscanf(range, "0x%hx-0x%hx", &low, &high) != 2 &&
        sscanf(range, "%hu-%hu", &low, &high) != 2) {
        if (sscanf(range, "0x%hx", &low) != 1 &&
            sscanf(range, "%hu", &low) != 1)
            return false;
        high = low;
    }

    add_xperm_rule_raw(db, src, tgt, cls, low, high, effect, invert);

    return true;
}

static bool add_type_rule(struct policydb *db, const char *s, const char *t,
                          const char *c, const char *d, int effect)
{
    struct type_datum *src, *tgt, *def;
    struct class_datum *cls;
    struct avtab_key key;
    struct avtab_node *node;

    src = hashtab_search(db->p_types.table, (char *)s);
    if (!src)
        return false;

    tgt = hashtab_search(db->p_types.table, (char *)t);
    if (!tgt)
        return false;

    cls = hashtab_search(db->p_classes.table, (char *)c);
    if (!cls)
        return false;

    def = hashtab_search(db->p_types.table, (char *)d);
    if (!def)
        return false;

    key.source_type = src->value;
    key.target_type = tgt->value;
    key.target_class = cls->value;
    key.specified = effect;

    node = get_avtab_node(db, &key, NULL);
    if (!node)
        return false;

    node->datum.u.data = def->value;

    return true;
}

static bool add_filename_trans(struct policydb *db, const char *s,
                               const char *t, const char *c, const char *d,
                               const char *o)
{
    struct type_datum *src, *tgt, *otype;
    struct class_datum *cls;
    struct filename_trans_key key;

    src = hashtab_search(db->p_types.table, (char *)s);
    if (!src)
        return false;

    tgt = hashtab_search(db->p_types.table, (char *)t);
    if (!tgt)
        return false;

    cls = hashtab_search(db->p_classes.table, (char *)c);
    if (!cls)
        return false;

    otype = hashtab_search(db->p_types.table, (char *)o);
    if (!otype)
        return false;

    key.ttype = src->value;
    key.tclass = cls->value;
    key.name = (char *)d;

    struct filename_trans_datum *trans = policydb_filenametr_search(db, &key);
    if (trans) {
        if (ebitmap_get_bit(&trans->stypes, src->value - 1)) {
            trans->otype = otype->value;
            return true;
        }
    }

    struct filename_trans_datum *last = trans;
    trans = ksu_kvzalloc(sizeof(struct filename_trans_datum), GFP_ATOMIC);
    if (!trans)
        return false;
    trans->next = last;
    trans->otype = otype->value;

    if (hashtab_insert(db->filename_trans, &key, trans)) {
        ksu_kvfree(trans);
        return false;
    }

    db->compat_filename_trans_count++;
    return ebitmap_set_bit(&trans->stypes, src->value - 1, 1) == 0;
}

static bool add_genfscon(struct policydb *db, const char *fs_name,
                         const char *path, const char *context)
{
    struct genfs *genfs_p, *newgenfs;
    struct ocontext *newc, *c, *head;
    char *s;
    int len;

    s = kksprintf(GFP_ATOMIC, "u:r:%s:s0", context);
    if (!s)
        return false;

    newc = ksu_kvzalloc(sizeof(struct ocontext), GFP_ATOMIC);
    if (!newc) {
        ksu_kvfree(s);
        return false;
    }

    if (policydb_context_isvalid(db, &newc->context[0])) {
        // TODO: implement this?
    }

    newc->u.name = (char *)path;

    for (genfs_p = db->genfs; genfs_p; genfs_p = genfs_p->next) {
        if (strcmp(fs_name, genfs_p->fstype) == 0)
            break;
    }

    if (!genfs_p) {
        newgenfs = ksu_kvzalloc(sizeof(struct genfs), GFP_ATOMIC);
        if (!newgenfs) {
            ksu_kvfree(s);
            ksu_kvfree(newc);
            return false;
        }
        newgenfs->fstype = (char *)fs_name;
        newgenfs->next = db->genfs;
        db->genfs = newgenfs;
        genfs_p = newgenfs;
    }

    head = genfs_p->head;
    for (c = head; c; c = c->next) {
        if (strcmp(path, c->u.name) == 0 &&
            (!c->v.sclass || c->v.sclass == SECCLASS_FILE))
            break;
    }

    if (c) {
        ksu_kvfree(s);
        ksu_kvfree(newc);
        return true;
    }

    newc->next = head;
    genfs_p->head = newc;

    return true;
}

#ifdef KSU_SUPPORT_ADD_TYPE
#define ksu_kvrealloc(p, old_size, new_size)                                   \
    ksu_kvrealloc_compat(p, old_size, new_size, GFP_ATOMIC)

static bool add_type(struct policydb *db, const char *type_name, bool attr)
{
    struct type_datum *type;
    char *key;
    u32 value;

    if (hashtab_search(db->p_types.table, (char *)type_name))
        return true;

    type = ksu_kvzalloc(sizeof(struct type_datum), GFP_ATOMIC);
    if (!type)
        return false;

    key = kksprintf(GFP_ATOMIC, "%s", type_name);
    if (!key) {
        ksu_kvfree(type);
        return false;
    }

    db->p_types.nprim++;
    value = db->p_types.nprim;
    type->value = value;
    type->primary = 1;
    type->attribute = attr ? 1 : 0;

    if (hashtab_insert(db->p_types.table, key, type)) {
        ksu_kvfree(key);
        ksu_kvfree(type);
        return false;
    }

    if (ksu_kvrealloc(db->type_val_to_struct,
                      sizeof(*db->type_val_to_struct) * (value - 1),
                      sizeof(*db->type_val_to_struct) * value) == NULL)
        return false;

    struct ebitmap *new_type_attr_map_array =
        ksu_kvzalloc(sizeof(struct ebitmap) * value, GFP_ATOMIC);
    if (!new_type_attr_map_array)
        return false;

    memcpy(new_type_attr_map_array, db->type_attr_map_array,
           sizeof(struct ebitmap) * (value - 1));
    ksu_kvfree(db->type_attr_map_array);

    db->type_attr_map_array = new_type_attr_map_array;
    ebitmap_init(&db->type_attr_map_array[value - 1]);
    ebitmap_set_bit(&db->type_attr_map_array[value - 1], value - 1, 1);

    db->type_val_to_struct = (struct type_datum **)new_type_val_to_struct;
    db->type_val_to_struct[value - 1] = type;

    char **new_val_to_name_types =
        ksu_kvzalloc(sizeof(char *) * value, GFP_ATOMIC);
    memcpy(new_val_to_name_types, db->sym_val_to_name[SYM_TYPES],
           sizeof(char *) * (value - 1));
    ksu_kvfree(db->sym_val_to_name[SYM_TYPES]);
    db->sym_val_to_name[SYM_TYPES] = new_val_to_name_types;
    db->sym_val_to_name[SYM_TYPES][value - 1] = key;

    return true;
}
#endif

static bool set_type_state(struct policydb *db, const char *type_name,
                           bool permissive)
{
    struct type_datum *type;

    type = hashtab_search(db->p_types.table, (char *)type_name);
    if (!type)
        return false;

    if (permissive) {
        if (ebitmap_set_bit(&db->permissive_map, type->value, 1))
            return false;
    } else {
        ebitmap_set_bit(&db->permissive_map, type->value, 0);
    }

    return true;
}

static void add_typeattribute_raw(struct policydb *db, struct type_datum *type,
                                  struct type_datum *attr)
{
    struct ebitmap *sattr = &db->type_attr_map_array[type->value - 1];
    ebitmap_set_bit(sattr, attr->value - 1, 1);
}

//////////////////////////////////////////////////////
// Public API
//////////////////////////////////////////////////////

bool ksu_type(struct policydb *db, const char *name, const char *attr)
{
#ifdef KSU_SUPPORT_ADD_TYPE
    if (!add_type(db, name, false))
        return false;
    if (attr)
        return ksu_typeattribute(db, name, attr);
    return true;
#else
    return false;
#endif
}

bool ksu_attribute(struct policydb *db, const char *name)
{
#ifdef KSU_SUPPORT_ADD_TYPE
    return add_type(db, name, true);
#else
    return false;
#endif
}

bool ksu_permissive(struct policydb *db, const char *type)
{
    return set_type_state(db, type, true);
}

bool ksu_enforce(struct policydb *db, const char *type)
{
    return set_type_state(db, type, false);
}

bool ksu_typeattribute(struct policydb *db, const char *type, const char *attr)
{
    struct type_datum *type_d, *attr_d;

    type_d = hashtab_search(db->p_types.table, (char *)type);
    if (!type_d)
        return false;

    attr_d = hashtab_search(db->p_types.table, (char *)attr);
    if (!attr_d)
        return false;

    add_typeattribute_raw(db, type_d, attr_d);

    return true;
}

bool ksu_exists(struct policydb *db, const char *type)
{
    return hashtab_search(db->p_types.table, (char *)type) != NULL;
}

bool ksu_allow(struct policydb *db, const char *src, const char *tgt,
               const char *cls, const char *perm)
{
    return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, false);
}

bool ksu_deny(struct policydb *db, const char *src, const char *tgt,
              const char *cls, const char *perm)
{
    return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, true);
}

bool ksu_auditallow(struct policydb *db, const char *src, const char *tgt,
                    const char *cls, const char *perm)
{
    return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITALLOW, false);
}

bool ksu_dontaudit(struct policydb *db, const char *src, const char *tgt,
                   const char *cls, const char *perm)
{
    return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITDENY, true);
}

bool ksu_allowxperm(struct policydb *db, const char *src, const char *tgt,
                    const char *cls, const char *range)
{
    return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_ALLOWED, false);
}

bool ksu_auditallowxperm(struct policydb *db, const char *src, const char *tgt,
                         const char *cls, const char *range)
{
    return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_AUDITALLOW,
                          false);
}

bool ksu_dontauditxperm(struct policydb *db, const char *src, const char *tgt,
                        const char *cls, const char *range)
{
    return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_AUDITDENY,
                          true);
}

bool ksu_type_transition(struct policydb *db, const char *src, const char *tgt,
                         const char *cls, const char *def)
{
    return add_type_rule(db, src, tgt, cls, def, AVTAB_TRANSITION);
}

bool ksu_type_change(struct policydb *db, const char *src, const char *tgt,
                     const char *cls, const char *def)
{
    return add_type_rule(db, src, tgt, cls, def, AVTAB_CHANGE);
}

bool ksu_type_member(struct policydb *db, const char *src, const char *tgt,
                     const char *cls, const char *def)
{
    return add_type_rule(db, src, tgt, cls, def, AVTAB_MEMBER);
}

bool ksu_genfscon(struct policydb *db, const char *fs_name, const char *path,
                  const char *context)
{
    return add_genfscon(db, fs_name, path, context);
}

#else

// Stubs for kernels < 5.0.0

bool ksu_type(struct policydb *db, const char *name, const char *attr)
{
    return true;
}

bool ksu_attribute(struct policydb *db, const char *name)
{
    return true;
}

bool ksu_permissive(struct policydb *db, const char *type)
{
    return true;
}

bool ksu_enforce(struct policydb *db, const char *type)
{
    return true;
}

bool ksu_typeattribute(struct policydb *db, const char *type, const char *attr)
{
    return true;
}

bool ksu_exists(struct policydb *db, const char *type)
{
    return true;
}

bool ksu_allow(struct policydb *db, const char *src, const char *tgt,
               const char *cls, const char *perm)
{
    return true;
}

bool ksu_deny(struct policydb *db, const char *src, const char *tgt,
              const char *cls, const char *perm)
{
    return true;
}

bool ksu_auditallow(struct policydb *db, const char *src, const char *tgt,
                    const char *cls, const char *perm)
{
    return true;
}

bool ksu_dontaudit(struct policydb *db, const char *src, const char *tgt,
                   const char *cls, const char *perm)
{
    return true;
}

bool ksu_allowxperm(struct policydb *db, const char *src, const char *tgt,
                    const char *cls, const char *range)
{
    return true;
}

bool ksu_auditallowxperm(struct policydb *db, const char *src, const char *tgt,
                         const char *cls, const char *range)
{
    return true;
}

bool ksu_dontauditxperm(struct policydb *db, const char *src, const char *tgt,
                        const char *cls, const char *range)
{
    return true;
}

bool ksu_type_transition(struct policydb *db, const char *src, const char *tgt,
                         const char *cls, const char *def, const char *obj)
{
    return true;
}

bool ksu_type_change(struct policydb *db, const char *src, const char *tgt,
                     const char *cls, const char *def)
{
    return true;
}

bool ksu_type_member(struct policydb *db, const char *src, const char *tgt,
                     const char *cls, const char *def)
{
    return true;
}

bool ksu_genfscon(struct policydb *db, const char *fs_name, const char *path,
                  const char *context)
{
    return true;
}

#endif
