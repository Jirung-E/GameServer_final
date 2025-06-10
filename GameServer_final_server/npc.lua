myid = 99999;

target_id = nil;
trigger_time = 0;
move_dir = {x = 0, y = 0};


function set_uid(id)
    myid = id;
end


function get_random_direction()
    local dirs = {
        {x = 1, y = 0},
        {x = -1, y = 0},
        {x = 0, y = 1},
        {x = 0, y = -1}
    }
    return dirs[math.random(1, #dirs)]
end


function on_trigger(p_id, time) 
    trigger_time = time;
    target_id = p_id;
    move_dir = get_random_direction();
    API_sendMessage(myid, p_id, "HELLO");
end


-- time: ms
function ai_update(x, y, near_players, time)
    if(target_id == nil) then
        move_dir = get_random_direction();
    end

    API_move(myid, move_dir.x, move_dir.y);

    local x = API_get_x(myid);
    local y = API_get_y(myid);

    for _, p_id in ipairs(near_players) do
        local player_x = API_get_x(p_id);
        local player_y = API_get_y(p_id);

        if(player_x == x and player_y == y) then
            on_trigger(p_id, time);
        end
    end

    if(target_id ~= nil and time - trigger_time >= 3000) then
        API_sendMessage(myid, target_id, "BYE");
        target_id = nil;
    end
end


function event_player_move(x, y, p_id, time)
    local player_x = API_get_x(p_id);
    local player_y = API_get_y(p_id);

    if(player_x == x and player_y == y) then
        on_trigger(p_id, time);
    end
end