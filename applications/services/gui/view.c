#include "view_i.h"

View* view_alloc() {
    View* view = malloc(sizeof(View));
    view->orientation = ViewOrientationHorizontal;
    return view;
}

void view_free(View* view) {
    furry_assert(view);
    view_free_model(view);
    free(view);
}

void view_tie_icon_animation(View* view, IconAnimation* icon_animation) {
    furry_assert(view);
    icon_animation_set_update_callback(icon_animation, view_icon_animation_callback, view);
}

void view_set_draw_callback(View* view, ViewDrawCallback callback) {
    furry_assert(view);
    furry_assert(view->draw_callback == NULL);
    view->draw_callback = callback;
}

void view_set_input_callback(View* view, ViewInputCallback callback) {
    furry_assert(view);
    furry_assert(view->input_callback == NULL);
    view->input_callback = callback;
}

void view_set_custom_callback(View* view, ViewCustomCallback callback) {
    furry_assert(view);
    furry_assert(callback);
    view->custom_callback = callback;
}

void view_set_previous_callback(View* view, ViewNavigationCallback callback) {
    furry_assert(view);
    view->previous_callback = callback;
}

void view_set_enter_callback(View* view, ViewCallback callback) {
    furry_assert(view);
    view->enter_callback = callback;
}

void view_set_exit_callback(View* view, ViewCallback callback) {
    furry_assert(view);
    view->exit_callback = callback;
}

void view_set_update_callback(View* view, ViewUpdateCallback callback) {
    furry_assert(view);
    view->update_callback = callback;
}

void view_set_update_callback_context(View* view, void* context) {
    furry_assert(view);
    view->update_callback_context = context;
}

void view_set_context(View* view, void* context) {
    furry_assert(view);
    furry_assert(context);
    view->context = context;
}

void view_set_orientation(View* view, ViewOrientation orientation) {
    furry_assert(view);
    view->orientation = orientation;
}

void view_allocate_model(View* view, ViewModelType type, size_t size) {
    furry_assert(view);
    furry_assert(size > 0);
    furry_assert(view->model_type == ViewModelTypeNone);
    furry_assert(view->model == NULL);
    view->model_type = type;
    if(view->model_type == ViewModelTypeLockFree) {
        view->model = malloc(size);
    } else if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = malloc(sizeof(ViewModelLocking));
        model->mutex = furry_mutex_alloc(FurryMutexTypeRecursive);
        furry_check(model->mutex);
        model->data = malloc(size);
        view->model = model;
    } else {
        furry_crash(NULL);
    }
}

void view_free_model(View* view) {
    furry_assert(view);
    if(view->model_type == ViewModelTypeNone) {
        return;
    } else if(view->model_type == ViewModelTypeLockFree) {
        free(view->model);
    } else if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = view->model;
        furry_mutex_free(model->mutex);
        free(model->data);
        free(model);
        view->model = NULL;
    } else {
        furry_crash(NULL);
    }
}

void* view_get_model(View* view) {
    furry_assert(view);
    if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = (ViewModelLocking*)(view->model);
        furry_check(furry_mutex_acquire(model->mutex, FurryWaitForever) == FurryStatusOk);
        return model->data;
    }
    return view->model;
}

void view_commit_model(View* view, bool update) {
    furry_assert(view);
    view_unlock_model(view);
    if(update && view->update_callback) {
        view->update_callback(view, view->update_callback_context);
    }
}

void view_icon_animation_callback(IconAnimation* instance, void* context) {
    UNUSED(instance);
    furry_assert(context);
    View* view = context;
    if(view->update_callback) {
        view->update_callback(view, view->update_callback_context);
    }
}

void view_unlock_model(View* view) {
    furry_assert(view);
    if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = (ViewModelLocking*)(view->model);
        furry_check(furry_mutex_release(model->mutex) == FurryStatusOk);
    }
}

void view_draw(View* view, Canvas* canvas) {
    furry_assert(view);
    if(view->draw_callback) {
        void* data = view_get_model(view);
        view->draw_callback(canvas, data);
        view_unlock_model(view);
    }
}

bool view_input(View* view, InputEvent* event) {
    furry_assert(view);
    if(view->input_callback) {
        return view->input_callback(event, view->context);
    } else {
        return false;
    }
}

bool view_custom(View* view, uint32_t event) {
    furry_assert(view);
    if(view->custom_callback) {
        return view->custom_callback(event, view->context);
    } else {
        return false;
    }
}

uint32_t view_previous(View* view) {
    furry_assert(view);
    if(view->previous_callback) {
        return view->previous_callback(view->context);
    } else {
        return VIEW_IGNORE;
    }
}

void view_enter(View* view) {
    furry_assert(view);
    if(view->enter_callback) view->enter_callback(view->context);
}

void view_exit(View* view) {
    furry_assert(view);
    if(view->exit_callback) view->exit_callback(view->context);
}
