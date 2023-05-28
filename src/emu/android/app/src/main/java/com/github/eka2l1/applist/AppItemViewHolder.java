package com.github.eka2l1.applist;

import static com.github.eka2l1.emu.Constants.KEY_APP_NAME;
import static com.github.eka2l1.emu.Constants.KEY_APP_UID;

import android.content.Intent;
import android.view.ContextMenu;
import android.view.MenuInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.RecyclerView;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.EmulatorActivity;

public class AppItemViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener, View.OnCreateContextMenuListener {
    ImageView icon;
    TextView name;
    TextView uid;

    private AppItem item;
    private int adapterPosition;

    public static int contextIndex;

    public AppItemViewHolder(@NonNull View itemView) {
        super(itemView);

        name = itemView.findViewById(R.id.appName);
        icon = itemView.findViewById(R.id.iconView);
        uid = itemView.findViewById(R.id.appUid);

        itemView.setOnClickListener(this);
        itemView.setOnCreateContextMenuListener(this);
    }

    public void setItem(AppItem item, int adapterPosition) {
        this.item = item;
        this.adapterPosition = adapterPosition;

        name.setText(item.getTitle());
        if (uid != null) {
            uid.setText(String.format("UID: 0x%X", item.getUid()));
        }

        if (item.getIcon() != null) {
            icon.setImageBitmap(item.getIcon());
        } else {
            icon.setImageResource(R.mipmap.ic_ducky_foreground);
        }
    }

    @Override
    public void onClick(View v) {
        Intent intent = new Intent(v.getContext(), EmulatorActivity.class);
        intent.putExtra(KEY_APP_UID, item.getUid());
        intent.putExtra(KEY_APP_NAME, item.getTitle());

        Fragment fragment = FragmentManager.findFragment(v);
        fragment.startActivity(intent);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        contextIndex = adapterPosition;

        Fragment fragment = FragmentManager.findFragment(v);
        MenuInflater inflater = fragment.requireActivity().getMenuInflater();
        inflater.inflate(R.menu.context_appslist, menu);
    }
}
